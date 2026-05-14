#pragma once
#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;

class IRenderer {
public:
    // Virtual destructor declaration
    virtual ~IRenderer();

    // Initializes the DX-GL interop layer; must be called once after the D3D11 device is created
    virtual void InitDXInterop(ID3D11Device* device, ID3D11DeviceContext* ctx) = 0;

    // Zero-copy texture update: copies NV12 planes from the D3D11 decoder surface to
    // pre-registered staging textures entirely on the GPU - no CPU involvement
    virtual void UpdateVideoTexturesFromD3D(int slot, void* d3dTex, int arrayIndex, int width, int height) = 0;

    // Rendering & Frame Control
    virtual void Render(unsigned int shaderProgramID, int slot) = 0;
    virtual void SwapBuffers() = 0;
    virtual void PollEvents() = 0;

    // Window State
    virtual bool ShouldClose() = 0;
    virtual void ToggleFullscreen() = 0;
};