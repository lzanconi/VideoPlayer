#pragma once
#include <vector>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "IRenderer.h"

class GLRenderer : public IRenderer
{
public:
    GLRenderer(int width, int height, const char* title);
    virtual ~GLRenderer();

    // Transfers decoded frame data to OpenGL textures
    void UpdateVideoTextures(int slot, int w, int h, int lsY, uint8_t* dY, int lsUV, uint8_t* dUV) override;

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

    GLFWwindow* window;
    unsigned int VAO, VBO, EBO;
    unsigned int yTex, uvTex;   // Slot 1 (Foreground)
    unsigned int yTex2, uvTex2; // Slot 0 (Background)
    bool isFullscreen = false;
    int winX = 100, winY = 100, winW = 1280, winH = 720;
};