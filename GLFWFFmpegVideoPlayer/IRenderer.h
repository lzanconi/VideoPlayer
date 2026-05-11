#pragma once
#include <cstdint>

class IRenderer {
public:
    // Virtual destructor declaration
    virtual ~IRenderer();

    // Texture Updates
    virtual void UpdateVideoTextures(int slot, int w, int h, int lsY, uint8_t* dY, int lsUV, uint8_t* dUV) = 0;

    // Rendering & Frame Control
    virtual void Render(unsigned int shaderProgramID, int slot) = 0;
    virtual void SwapBuffers() = 0;
    virtual void PollEvents() = 0;

    // Window State
    virtual bool ShouldClose() = 0;
    virtual void ToggleFullscreen() = 0;
};