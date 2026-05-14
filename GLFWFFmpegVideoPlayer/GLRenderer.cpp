#include "GLRenderer.h"
#include <stdexcept>
#include "App.h"

GLRenderer::GLRenderer(int width, int height, const char* title)
{
    //Initializes the GLFW library; exits if initialization fails
    if (!glfwInit())
        exit(-1);

    //Creates a windowed mode window and its OpenGL context
    window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(-1);
    }

    ToggleFullscreen(); // Start in fullscreen mode

    //Make this window the current OpenGL context
    glfwMakeContextCurrent(window);
    //Load OpenGL function pointers using GLAD
    gladLoaderLoadGL();
    //Enable V-Sync - 0 to disable
    glfwSwapInterval(1);

    //Enables alpha blending to allow for video transparency/cross-fading
    glEnable(GL_BLEND);
    //Defines the blending math: (Source * Alpha) + (Destination * (1 - Alpha))
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Configures the rectangle geometry (quad) used to display videos
    SetupGeometry();
    //Generates and configures the OpenGL texture objects
    SetupTextures();
}

GLRenderer::~GLRenderer()
{
    for (int i = 0; i < 2; i++)
    {
        SlotInteropState& s = interopSlots[i];
        if (s.yGLTex)       glDeleteTextures(1, &s.yGLTex);
        if (s.uvGLTex)      glDeleteTextures(1, &s.uvGLTex);
        if (s.nv12Staging)  s.nv12Staging->Release();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glfwDestroyWindow(window);
    glfwTerminate();
}

/*
 * Stores the D3D11 device and immediate context that FFmpeg created for hardware decoding.
 * These are used later by UpdateVideoTexturesFromD3D to copy decoded frames into the
 * per-slot NV12 staging textures.
 */
void GLRenderer::InitDXInterop(ID3D11Device* device, ID3D11DeviceContext* ctx)
{
    d3dDevice  = device;
    d3dContext = ctx;
}

/*
 * Lazily creates (or recreates on resolution change) the per-slot resources:
 *   - One NV12 STAGING texture: written by a GPU-side D3D11 copy, then mapped
 *     on the CPU to retrieve plane pointers for glTexSubImage2D.
 *   - Two GL textures: GL_R8 for the Y plane, GL_RG8 for the UV plane.
 *
 * The NV12 STAGING texture is the key design decision. D3D11 only guarantees
 * CopySubresourceRegion compatibility between textures of the same format.
 * Copying NV12 subresources into R8/RG8 textures is NOT guaranteed by the spec
 * and silently fails on most hardware, producing a zeroed UV texture (green screen).
 * Copying NV12 → NV12 (same format) is always correct.
 */
void GLRenderer::EnsureInteropSlot(int slot, int width, int height)
{
    SlotInteropState& s = interopSlots[slot];

    // No recreation needed if dimensions have not changed
    if (s.width == width && s.height == height)
        return;

    // Release any previous resources
    if (s.yGLTex)      { glDeleteTextures(1, &s.yGLTex);  s.yGLTex  = 0; }
    if (s.uvGLTex)     { glDeleteTextures(1, &s.uvGLTex); s.uvGLTex = 0; }
    if (s.nv12Staging) { s.nv12Staging->Release(); s.nv12Staging = nullptr; }

    // NV12 STAGING texture: GPU-writable via CopySubresourceRegion, CPU-readable via Map
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = (UINT)width;
    desc.Height           = (UINT)height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags   = D3D11_CPU_ACCESS_READ;
    desc.BindFlags        = 0;
    desc.MiscFlags        = 0;
    if (FAILED(d3dDevice->CreateTexture2D(&desc, nullptr, &s.nv12Staging)))
        throw std::runtime_error("Failed to create NV12 staging texture");

    // GL textures: pre-allocated storage so glTexSubImage2D can update in-place each frame
    glGenTextures(1, &s.yGLTex);
    glGenTextures(1, &s.uvGLTex);

    auto initTex = [](GLuint t, int w, int h, GLint internalFmt, GLenum fmt)
    {
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, nullptr);
    };
    initTex(s.yGLTex,  width,     height,     GL_R8,  GL_RED);
    initTex(s.uvGLTex, width / 2, height / 2, GL_RG8, GL_RG);

    s.width  = width;
    s.height = height;
}

/*
 * Transfers decoded NV12 frame data into the per-slot GL textures.
 *
 * Step 1 — GPU copy (NV12 → NV12, same format, always compatible in D3D11):
 *   CopySubresourceRegion copies the Y and UV planes from the decoder's NV12 texture
 *   array slice into the NV12 staging texture. Same-format copies are the only kind
 *   guaranteed by the D3D11 spec. Copies from NV12 planes to R8/RG8 textures are NOT
 *   guaranteed — they silently produce no output on most hardware, which leaves the UV
 *   texture zeroed and makes the BT.709 shader output pure green.
 *
 * Step 2 — CPU map (pointer-only, no pixel copy):
 *   Map() gives the CPU a pointer to the staging texture's memory without copying it.
 *   The GPU stalls here until the copy above is complete (implicit sync).
 *
 * Step 3 — GL upload via glTexSubImage2D:
 *   The mapped pointers are passed directly to glTexSubImage2D, which DMA-transfers
 *   the data from the staging texture to the GL texture objects on the GPU.
 *
 * D3D11 NV12 texture array subresource layout (ArraySize = N, MipLevels = 1):
 *   - Y  plane for array element i : subresource i
 *   - UV plane for array element i : subresource N + i
 */
void GLRenderer::UpdateVideoTexturesFromD3D(int slot, void* d3dTex, int arrayIndex, int width, int height)
{
    EnsureInteropSlot(slot, width, height);
    SlotInteropState& s = interopSlots[slot];

    auto* srcTex = static_cast<ID3D11Texture2D*>(d3dTex);
    D3D11_TEXTURE2D_DESC srcDesc = {};
    srcTex->GetDesc(&srcDesc);

    // Subresource indices for the requested array element
    UINT ySubresource  = D3D11CalcSubresource(0, (UINT)arrayIndex,                    1);
    UINT uvSubresource = D3D11CalcSubresource(0, (UINT)arrayIndex + srcDesc.ArraySize, 1);

    // Step 1: GPU-side NV12→NV12 copy.
    // pSrcBox is nullptr so D3D11 copies the entire source subresource automatically.
    // This eliminates all ambiguity about whether NV12 plane box extents must be expressed
    // in luma or chroma coordinate space — a source of silent failures and green tinting.
    d3dContext->CopySubresourceRegion(s.nv12Staging, 0, 0, 0, 0, srcTex, ySubresource,  nullptr);
    d3dContext->CopySubresourceRegion(s.nv12Staging, 1, 0, 0, 0, srcTex, uvSubresource, nullptr);

    // Map the entire NV12 surface through subresource 0 only.
    // When an NV12 staging texture is mapped on the CPU, D3D11 packs both planes
    // into a single contiguous buffer exposed via subresource 0:
    //   Y  plane: pData + 0
    //   UV plane: pData + RowPitch * height
    // Mapping subresource 1 separately always returns E_INVALIDARG on NV12 staging textures.
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (FAILED(d3dContext->Map(s.nv12Staging, 0, D3D11_MAP_READ, 0, &mapped)))
        return;

    auto* base  = static_cast<uint8_t*>(mapped.pData);
    auto* yPtr  = base;
    auto* uvPtr = base + mapped.RowPitch * height; // UV plane starts immediately after Y

    // Y plane: R8 — 1 byte per pixel, so GL_UNPACK_ROW_LENGTH = RowPitch
    glActiveTexture(GL_TEXTURE0 + (slot * 2));
    glBindTexture(GL_TEXTURE_2D, s.yGLTex);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)mapped.RowPitch);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, yPtr);

    // UV plane: RG8 — 2 bytes per pixel (U and V interleaved), so GL_UNPACK_ROW_LENGTH = RowPitch / 2
    glActiveTexture(GL_TEXTURE1 + (slot * 2));
    glBindTexture(GL_TEXTURE_2D, s.uvGLTex);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(mapped.RowPitch / 2));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RG, GL_UNSIGNED_BYTE, uvPtr);

    d3dContext->Unmap(s.nv12Staging, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

/*
* The Render function is the final stage of the drawing pipeline, where the textures updated by the decoder are
* actually drawn onto the screen.
* It coordinates the shader, geometry, and texture units to produce the final image.
*
* 1.Viewport synchronization
* Before drawing, the function ensures the OpenGL viewport matches the current window size.
* This prevents the video from appearing stretched or incorrectly positioned if the user resizes the window or toggles fullscreen mode.
*
* 2.Shader and Texture mapping
* The function uses the slot parameter to tell the GPU which set of video textures (Background or Foreground) to use for the current draw call
*
* 3.Drawing the geometry
* Once the textures are mapped, the function commands the GPU to draw the quad
*/
void GLRenderer::Render(unsigned int shaderProgramID, int slot)
{
    //1.Viewport synchronization
    int dw, dh;
    glfwGetFramebufferSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);

    glUseProgram(shaderProgramID);
    glUniform1i(glGetUniformLocation(shaderProgramID, "uRotated"), App::state.isRotated ? 1 : 0);

    //Map shader uniforms to correct texture units based on slot
    glUniform1i(glGetUniformLocation(shaderProgramID, "yTexture"),  slot * 2);
    glUniform1i(glGetUniformLocation(shaderProgramID, "uvTexture"), (slot * 2) + 1);

    SlotInteropState& s = interopSlots[slot];
    if (s.yGLTex && s.uvGLTex)
    {
        // Bind the GL textures to the correct texture units for this slot
        // (slot 0 → units 0,1 ; slot 1 → units 2,3)
        glActiveTexture(GL_TEXTURE0 + (slot * 2));
        glBindTexture(GL_TEXTURE_2D, s.yGLTex);
        glActiveTexture(GL_TEXTURE1 + (slot * 2));
        glBindTexture(GL_TEXTURE_2D, s.uvGLTex);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
}


void GLRenderer::ToggleFullscreen()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if (!isFullscreen) 
    {
        glfwGetWindowPos(window, &winX, &winY);
        glfwGetWindowSize(window, &winW, &winH);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        // Hide and lock the cursor to the window
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else 
    {
        glfwSetWindowMonitor(window, NULL, winX, winY, winW, winH, 0);
        // Restore the cursor to normal visibility
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    
    isFullscreen = !isFullscreen;
    glfwSwapInterval(1);
}

void GLRenderer::SetupGeometry()
{
    float vertices[] = {
     1.f,  1.f, 0.f,  1.f, 1.f,
     1.f, -1.f, 0.f,  1.f, 0.f,
    -1.f, -1.f, 0.f,  0.f, 0.f,
    -1.f,  1.f, 0.f,  0.f, 1.f
    };
    unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void GLRenderer::SetupTextures()
{
    // Textures are created lazily per-slot in EnsureInteropSlot() once the D3D11 device
    // and video resolution are known. Nothing to allocate here.
}

void GLRenderer::SetKeyCallback(GLFWkeyfun cb) { glfwSetKeyCallback(window, cb); }
bool GLRenderer::ShouldClose() { return glfwWindowShouldClose(window); }
void GLRenderer::SwapBuffers() { glfwSwapBuffers(window); }
void GLRenderer::PollEvents() { glfwPollEvents(); }
