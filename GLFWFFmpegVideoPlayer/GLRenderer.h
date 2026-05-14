#pragma once
#include <vector>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <d3d11.h>

#include "IRenderer.h"

// Holds per-slot D3D11 and GL resources for NV12 plane upload
struct SlotInteropState
{
    // CPU-readable NV12 staging texture. The GPU copies the decoded NV12 frame
    // into this texture (NV12?NV12, same format — always compatible in D3D11),
    // then the CPU maps it to obtain plane pointers for glTexSubImage2D upload.
    ID3D11Texture2D* nv12Staging = nullptr;
    GLuint yGLTex  = 0; // GL_R8   — Y  (luminance) plane
    GLuint uvGLTex = 0; // GL_RG8  — UV (chrominance) plane
    int width  = 0;
    int height = 0;
};

class GLRenderer : public IRenderer
{
public:
    GLRenderer(int width, int height, const char* title);
    virtual ~GLRenderer();

    // Stores the D3D11 device and context provided by FFmpeg's hardware device context
    void InitDXInterop(ID3D11Device* device, ID3D11DeviceContext* ctx) override;

    // Copies the decoded NV12 frame to a staging texture, maps it on the CPU,
    // and uploads the Y and UV planes to GL via glTexSubImage2D
    void UpdateVideoTexturesFromD3D(int slot, void* d3dTex, int arrayIndex, int width, int height) override;

    // Executes the final drawing pipeline stage
    void Render(unsigned int shaderProgramID, int slot) override;

    bool ShouldClose() override;
    void SwapBuffers() override;
    void PollEvents() override;
    void ToggleFullscreen() override;

    void SetKeyCallback(GLFWkeyfun cb);

private:
    void SetupGeometry();
    void SetupTextures();

    // Lazily creates or recreates per-slot resources when resolution changes
    void EnsureInteropSlot(int slot, int width, int height);

    GLFWwindow* window;
    unsigned int VAO, VBO, EBO;

    // Per-slot state (slot 0 = background, slot 1 = foreground)
    SlotInteropState interopSlots[2];

    // D3D11 device and immediate context from FFmpeg's AVD3D11VADeviceContext
    ID3D11Device*        d3dDevice  = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;

    bool isFullscreen = false;
    int winX = 100, winY = 100, winW = 1280, winH = 720;
};