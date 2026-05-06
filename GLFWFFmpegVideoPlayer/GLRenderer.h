#pragma once
#include <iostream>
#include <vector>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "IRenderer.h"

/*
* OPENGL RENDERER
* OpenGL implementation of the IRenderer interface
*/
class GLRenderer : public IRenderer
{
public:
    GLRenderer(int width, int height, const char* title)
    {
        //Init GLFW 
        if (!glfwInit())
            exit(-1);

        //Creates the application window
        window = glfwCreateWindow(width, height, title, NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            exit(-1);
        }

        //Set the window as the OpenGL context
        glfwMakeContextCurrent(window);
        //Initializes GLAD to load OpenGL extensions
        gladLoaderLoadGL();
        //1 --> Enables V-Sync / 0 --> No V-Sync
        glfwSwapInterval(1);

        // Enable blending for the cross-fade effect
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //Creates the quad where it renders the video texture
        SetupGeometry();
        
        //Prepares the GPU to receive video data
        SetupTextures();
    }

    virtual ~GLRenderer()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteTextures(1, &yTex);
        glDeleteTextures(1, &uvTex);
        glDeleteTextures(1, &yTex2);
        glDeleteTextures(1, &uvTex2);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    /*
    * Transfers decoded video frame data from CPU-accessible memory to OpenGL textures on the GPU
    * 
    * The player uses hardware acceleration (D3D11) to decode videos into the NV12 format.
    * NV12 formats separates a frame into two distinct buffers:
    * -Y (Luminance): contains the brightness/grayscale information at full resolution
    * -UV (Chrominance): contains the color information at half resolution (subsampled)
    * 
    * Because of this separation, UpdateVideoTextures must update two textures per frame rather than one.
    * 
    * The function uses a slot parameter to determine which video layer is being updated:
    * -Slot 0 (Background): updates yTex2 and uvTex2
    * -Slot 1 (Foreground): updates yTex and uvTex
    * 
    * The function calculates the "Active Texture" unit using the formula GL_TEXTURE0 + (slot * 2).
    * This ensures the background occupies units 0 and 1, foreground occupies units 2 and 3
    */
    void UpdateVideoTextures(int slot, int w, int h, int lsY, uint8_t* dY, int lsUV, uint8_t* dUV) override
    {
        //Tells OpenGL how to read the raw memory buffers, accounting for "padding" (linesize) that FFmpeg might 
        //add to the video frames for memory alignment.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        GLuint currentY = (slot == 0) ? yTex2 : yTex;
        GLuint currentUV = (slot == 0) ? uvTex2 : uvTex;

        glActiveTexture(GL_TEXTURE0 + (slot * 2));
        glBindTexture(GL_TEXTURE_2D, currentY);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, lsY);
        //Uploads the Y data using the GL_R8 (8-bit Red/Single Channel) format.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, dY);

        glActiveTexture(GL_TEXTURE1 + (slot * 2));
        glBindTexture(GL_TEXTURE_2D, currentUV);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, lsUV / 2);
        //Uploads the UV data using GL_RG8(Two channels : Red and Green) at half the width and height of the Y texture.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, w / 2, h / 2, 0, GL_RG, GL_UNSIGNED_BYTE, dUV);

        //It resets GL_UNPACK_ROW_LENGTH to 0 to prevent these video settings from affecting other OpenGL operations in the program
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
    void Render(unsigned int shaderProgramID, int slot) override
    {
        //1.Viewport synchronization
        int dw, dh;
        //Retrieves the actual pixel dimensions of the window.
        glfwGetFramebufferSize(window, &dw, &dh);
        //Maps the rendering coordinates to the window's pixels.
        glViewport(0, 0, dw, dh);

        //Activates the shader
        glUseProgram(shaderProgramID);

        //Maps texture units based on slot
        glUniform1i(glGetUniformLocation(shaderProgramID, "yTexture"), slot * 2);
        glUniform1i(glGetUniformLocation(shaderProgramID, "uvTexture"), (slot * 2) + 1);

        //Draws the geometry
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    bool ShouldClose() override 
    { 
        return glfwWindowShouldClose(window); 
    }

    void SwapBuffers() override 
    { 
        glfwSwapBuffers(window); 
    }

    void PollEvents() override 
    { 
        glfwPollEvents(); 
    }

    void ToggleFullscreen() override
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        if (!isFullscreen) {
            glfwGetWindowPos(window, &winX, &winY);
            glfwGetWindowSize(window, &winW, &winH);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else {
            glfwSetWindowMonitor(window, NULL, winX, winY, winW, winH, 0);
        }
        isFullscreen = !isFullscreen;
        glfwSwapInterval(1);
    }

    void SetKeyCallback(GLFWkeyfun cb) 
    { 
        glfwSetKeyCallback(window, cb); 
    }

private:
    GLFWwindow* window;
    unsigned int VAO, VBO, EBO;
    unsigned int yTex, uvTex;   // Slot 1 (Foreground)
    unsigned int yTex2, uvTex2; // Slot 0 (Background)
    bool isFullscreen = false;
    int winX = 100, winY = 100, winW = 1280, winH = 720;

    void SetupGeometry()
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

    void SetupTextures()
    {
        glGenTextures(1, &yTex);
        glGenTextures(1, &uvTex);
        glGenTextures(1, &yTex2);
        glGenTextures(1, &uvTex2);

        auto initTex = [](GLuint t) {
            glBindTexture(GL_TEXTURE_2D, t);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            };

        initTex(yTex); initTex(uvTex);
        initTex(yTex2); initTex(uvTex2);
    }
};