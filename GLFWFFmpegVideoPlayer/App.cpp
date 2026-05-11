#include "App.h"
#include "GLRenderer.h"
#include "ShaderProgram.h"
#include "VideoSource.h"
#include "ContentManager.h"
#include "NetworkManager.h"
#include <iostream>
#include <stdexcept>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavcodec/avcodec.h>
}

// Initialize the static AppState member
AppState App::state;

App::App(int width, int height, const std::string& title)
    : renderer(nullptr), videoShader(nullptr), hw_ctx(nullptr),
    pkt(nullptr), frm(nullptr), sw_frm(nullptr)
{
    // Load content from folder
    ContentManager contentMgr;
    contentMgr.LoadVideoContentFromFolder(".\\Videos");

    if (contentMgr.GetVideoContents().empty()) 
    {
        std::cerr << "No .mp4 files found." << std::endl;
    }

    // Initialize Renderer
    GLRenderer* concreteRenderer = new GLRenderer(width, height, title.c_str());
    concreteRenderer->SetKeyCallback(App::KeyCallback);
    renderer = concreteRenderer;
    state.renderer = renderer;

    // Setup Networking
    state.networkMgr = new NetworkManager("127.0.0.1", 5555, this);

    // Load Shaders
    videoShader = new ShaderProgram("shader.vert", "shader.frag");

    // Setup Hardware Acceleration (D3D11)
    if (av_hwdevice_ctx_create(&hw_ctx, AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, 0) < 0) 
    {
        throw std::runtime_error("Failed to create HW Device Context");
    }

    // Initialize Video Sources from content
    for (const auto& videoContent : contentMgr.GetVideoContents()) 
    {
        VideoSource* videoSource = new VideoSource();
        if (videoSource->Open(videoContent.filename, hw_ctx)) 
        {
            videoSource->SetFadeInDuration(videoContent.fadeInDuration);
            videoSource->SetFadeOutDuration(videoContent.fadeOutDuration);
            videoSource->SetLooped(videoContent.looped);
            videoSource->positions = videoContent.positions;
            state.sources.push_back(videoSource);
        }
        else 
        {
            delete videoSource;
        }
    }

    // Allocate shared FFmpeg buffers
    pkt = av_packet_alloc();
    frm = av_frame_alloc();
    sw_frm = av_frame_alloc();

    state.lastFPSUpdate = glfwGetTime();
    state.networkMgr->Start();
}

App::~App() 
{
    if (state.networkMgr) 
        delete state.networkMgr;
    if (renderer) 
        delete renderer;
    if (videoShader) 
        delete videoShader;
    for (auto source : state.sources) 
        delete source;

    av_frame_free(&frm);
    av_frame_free(&sw_frm);
    av_packet_free(&pkt);
    if (hw_ctx) 
        av_buffer_unref(&hw_ctx);
}

// IApp implementation
VideoSource* App::GetBackgroundVideo() 
{
    return state.sources.empty() ? nullptr : state.sources[0];
}

std::vector<float> App::GetPositions() 
{
    return state.sources[0]->positions;
}

double App::GetLastPTS() 
{
    return state.sources[0]->GetLastPTS();
}

int64_t App::GetBGCaptureTimeNS() 
{
    return state.sources[0]->GetBGCaptureTimeNS();
}

void App::Run() 
{
    while (!renderer->ShouldClose()) 
    {
        renderer->PollEvents();

        // Update Background (Slot 0)
        state.sources[0]->UpdateAndRender(renderer, videoShader, frm, sw_frm, pkt, 0);

        // Update Foreground if active (Slot 1)
        if (state.activeIndex != 0) 
        {
            VideoSource* foreground = state.sources[state.activeIndex];
            if (!foreground->UpdateAndRender(renderer, videoShader, frm, sw_frm, pkt, 1)) 
            {
                state.activeIndex = 0; // Return to background on completion
            }
        }

        renderer->SwapBuffers();

        // FPS Tracking
        state.frameCount++;
        double currentTime = glfwGetTime();
        if (currentTime - state.lastFPSUpdate >= 1.0) 
        {
            std::cout << "FPS: " << state.frameCount << std::endl;
            state.frameCount = 0;
            state.lastFPSUpdate = currentTime;
        }
    }
}

void App::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    if (action != GLFW_PRESS) 
        return;

	//Trigger foreground videos with left/right arrows, allowing cycling through multiple sources if available
    if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_LEFT) 
    {
        int dir = (key == GLFW_KEY_RIGHT) ? 1 : -1;
        int numSources = (int)state.sources.size();

        if (numSources <= 1) 
            return;

        int nextIdx = (state.lastForegroundIndex == 0) ? 1 : state.lastForegroundIndex + dir;
        if (nextIdx >= numSources) 
            nextIdx = 1;
        
        if (nextIdx < 1) 
            nextIdx = numSources - 1;

        state.activeIndex = nextIdx;
        state.lastForegroundIndex = nextIdx;
        state.sources[state.activeIndex]->Rewind();
        state.sources[state.activeIndex]->Play(glfwGetTime());
    }

	//Up arrow stop foreground video and quickly returns to the background video
    if (key == GLFW_KEY_UP) 
    {
        state.activeIndex = 0;
        state.sources[state.lastForegroundIndex]->Rewind();
    }

	//Enter key starts the first video if it's not already playing
    if (key == GLFW_KEY_ENTER && !state.sources.empty()) 
    {
        state.sources[0]->Play(glfwGetTime());
    }

	//Spacebar toggles pause/resume for both background and foreground videos simultaneously	
    if (key == GLFW_KEY_SPACE) 
    {
        double time = glfwGetTime();
        state.sources[0]->Pause(time);
        if (state.activeIndex != 0) state.sources[state.activeIndex]->Pause(time);
    }

	//F key toggles fullscreen mode for the application window
    if (key == GLFW_KEY_F && state.renderer) 
        state.renderer->ToggleFullscreen();

    if (key == GLFW_KEY_R) 
    {
        state.isRotated = !state.isRotated;
        std::cout << "Rotation toggled: " << (state.isRotated ? "ON" : "OFF") << std::endl;
	}

    //Escape key gracefully exits the application
    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, true);

}