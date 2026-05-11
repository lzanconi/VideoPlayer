#include "GLRenderer.h"
#include <iostream>

GLRenderer::GLRenderer(int width, int height, const char* title)
{
    if (!glfwInit())
        exit(-1);

    window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(-1);
    }

    glfwMakeContextCurrent(window);
    gladLoaderLoadGL();
    glfwSwapInterval(1); // Enable V-Sync

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    SetupGeometry();
    SetupTextures();
}

GLRenderer::~GLRenderer()
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

void GLRenderer::UpdateVideoTextures(int slot, int w, int h, int lsY, uint8_t* dY, int lsUV, uint8_t* dUV)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLuint currentY = (slot == 0) ? yTex2 : yTex;
    GLuint currentUV = (slot == 0) ? uvTex2 : uvTex;

    // Background uses units 0 and 1, foreground uses units 2 and 3
    glActiveTexture(GL_TEXTURE0 + (slot * 2));
    glBindTexture(GL_TEXTURE_2D, currentY);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, lsY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, dY);

    glActiveTexture(GL_TEXTURE1 + (slot * 2));
    glBindTexture(GL_TEXTURE_2D, currentUV);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, lsUV / 2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, w / 2, h / 2, 0, GL_RG, GL_UNSIGNED_BYTE, dUV);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void GLRenderer::Render(unsigned int shaderProgramID, int slot)
{
    int dw, dh;
    glfwGetFramebufferSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);

    glUseProgram(shaderProgramID);

    // Map shader uniforms to correct texture units based on slot
    glUniform1i(glGetUniformLocation(shaderProgramID, "yTexture"), slot * 2);
    glUniform1i(glGetUniformLocation(shaderProgramID, "uvTexture"), (slot * 2) + 1);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

bool GLRenderer::ShouldClose() { return glfwWindowShouldClose(window); }
void GLRenderer::SwapBuffers() { glfwSwapBuffers(window); }
void GLRenderer::PollEvents() { glfwPollEvents(); }

void GLRenderer::ToggleFullscreen()
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

void GLRenderer::SetKeyCallback(GLFWkeyfun cb) { glfwSetKeyCallback(window, cb); }

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