#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <string>
#include "IApp.h"
#include "customtypes.h"

// Forward declarations
class IRenderer;
class ShaderProgram;
class VideoSource;
class NetworkManager;
struct AVBufferRef;
struct AVPacket;
struct AVFrame;
struct GLFWwindow;

class App : public IApp
{
public:
    App(int width, int height, const std::string& title);
    ~App();

    // IApp Interface Overrides
    VideoSource* GetBackgroundVideo() override;
    std::vector<float> GetPositions() override;
    double GetLastPTS() override;
    int64_t GetBGCaptureTimeNS() override;

    // Main execution loop
    void Run();

public:
    static AppState state; // Static state shared with callbacks

private:
    IRenderer* renderer;
    ShaderProgram* videoShader;
    AVBufferRef* hw_ctx;
    AVPacket* pkt;
    AVFrame* frm;
    AVFrame* sw_frm;

    // Input handling
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
};