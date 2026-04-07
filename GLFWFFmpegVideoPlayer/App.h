#pragma once
#include <iostream>
#include <windows.h>
#include <d3d11.h>
#include <vector>

#include "customtypes.h"
#include "GLRenderer.h"
#include "ShaderProgram.h"
#include "VideoSource.h"

class App
{
public:
    App(int width, int height, const std::string& title, const std::vector<VideoContent>& videoContents)
    {
        // 1. Initialize Renderer and set the global reference
        renderer = new GLRenderer(width, height, title.c_str());
        renderer->SetKeyCallback(App::KeyCallback);
        state.renderer = renderer;

        // 2. Load Shaders
        videoShader = new ShaderProgram("shader.vert", "shader.frag");

        // 3. Setup Hardware Acceleration
        if (av_hwdevice_ctx_create(&hw_ctx, AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, 0) < 0) {
            throw std::runtime_error("Failed to create HW Device Context");
        }

        // 4. Initialize Video Sources
        for (const auto& videoContent : videoContents)
        {
            VideoSource* videoSource = new VideoSource();
            if (videoSource->Open(videoContent.filename, hw_ctx)) {
                videoSource->SetFadeInDuration(videoContent.fadeInDuration);
                videoSource->SetFadeOutDuration(videoContent.fadeOutDuration);
                videoSource->SetLooped(videoContent.looped);
                state.sources.push_back(videoSource);
            }
            else {
                delete videoSource;
            }
        }

        //Automatically plays the background video when app startup
        state.sources[0]->Play(glfwGetTime());

        // 5. Allocate shared decoding buffers
        pkt = av_packet_alloc();
        frm = av_frame_alloc();
        sw_frm = av_frame_alloc();

        state.lastFPSUpdate = glfwGetTime();
    }

    ~App()
    {
        if (renderer) delete renderer;
        if (videoShader) delete videoShader;
        for (auto source : state.sources) delete source;
        av_frame_free(&frm);
        av_frame_free(&sw_frm);
        av_packet_free(&pkt);
        if (hw_ctx) av_buffer_unref(&hw_ctx);
    }

    void Run()
    {
        while (!renderer->ShouldClose()) {
            renderer->PollEvents();

            // 1. Handle Background (Always index 0, Slot 0)
            state.sources[0]->UpdateAndRender(renderer, videoShader, frm, sw_frm, pkt, 0);

            // 2. Handle Foreground Interrupts (Slot 1)
            if (state.activeIndex != 0) {
                VideoSource* foreground = state.sources[state.activeIndex];

                // If UpdateAndRender returns false, it reached the end of file
                if (!foreground->UpdateAndRender(renderer, videoShader, frm, sw_frm, pkt, 1)) {
                    state.activeIndex = 0; // Return to background
                }
            }

            renderer->SwapBuffers();

            // FPS Tracking
            state.frameCount++;
            double currentTime = glfwGetTime();

            if (currentTime - state.lastFPSUpdate >= 1.0) {
                std::cout << "FPS: " << state.frameCount << std::endl;
                state.frameCount = 0;
                state.lastFPSUpdate = currentTime;
            }
        }
    }

private:
    static AppState state;
    GLRenderer* renderer;
    ShaderProgram* videoShader;
    AVBufferRef* hw_ctx = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frm = nullptr;
    AVFrame* sw_frm = nullptr;

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action != GLFW_PRESS) return;

        if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_LEFT) {
            int dir = (key == GLFW_KEY_RIGHT) ? 1 : -1;
            int numSources = (int)state.sources.size();

            if (numSources <= 1) return;

            int nextIdx;
            if (state.lastForegroundIndex == 0) {
                nextIdx = 1;
            }
            else {
                nextIdx = state.lastForegroundIndex + dir;
                if (nextIdx >= numSources) nextIdx = 1;
                if (nextIdx < 1) nextIdx = numSources - 1;
            }

            state.activeIndex = nextIdx;
            state.lastForegroundIndex = nextIdx;

            state.sources[state.activeIndex]->Rewind();
            state.sources[state.activeIndex]->Play(glfwGetTime());
        }

        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);

        // UPDATED PAUSE LOGIC: Toggles both background and active foreground
        if (key == GLFW_KEY_SPACE) {
            double time = glfwGetTime();
            state.sources[0]->Pause(time);
            if (state.activeIndex != 0) {
                state.sources[state.activeIndex]->Pause(time);
            }
        }

        if (key == GLFW_KEY_F && state.renderer) state.renderer->ToggleFullscreen();
    }
};