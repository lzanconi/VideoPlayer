#pragma once
#include <iostream>
#include <windows.h>
#include <d3d11.h>

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

        for (const auto& videoContent : videoContents)
        {
            VideoSource* videoSource = new VideoSource();
            if (videoSource->Open(videoContent.filename, hw_ctx)) {
                // Apply the fade duration from the playlist definition
                videoSource->SetFadeDuration(videoContent.fadeDuration);
                state.sources.push_back(videoSource);
            }
            else {
                delete videoSource;
            }
        }

        // 4. Allocate shared decoding buffers
        pkt = av_packet_alloc();
        frm = av_frame_alloc();
        sw_frm = av_frame_alloc();
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

    // New method to access specific video sources
    VideoSource* GetSource(int index) {
        if (index >= 0 && index < (int)state.sources.size()) {
            return state.sources[index];
        }
        return nullptr;
    }

    void Run()
    {
        while (!renderer->ShouldClose()) {
            renderer->PollEvents();

            VideoSource& current = *state.sources[state.activeIndex];
            if (current.IsPaused()) {
                Sleep(10);
                continue;
            }

            if (current.GetStartTime() <= 0) {
                current.SetStartTime(glfwGetTime());
            }

            // Calculate cross-fade alpha progress using the source's own duration
            double elapsed = glfwGetTime() - current.GetAdjustedStartTime();
            float currentFadeLimit = current.GetFadeDuration();
            float alpha = (currentFadeLimit > 0) ? (float)(elapsed / currentFadeLimit) : 1.0f;

            if (alpha >= 1.0f) {
                alpha = 1.0f;
                state.previousIndex = -1; // End the transition period
            }

            // --- RENDER PASS ---
            glClear(GL_COLOR_BUFFER_BIT);

            // 1. Render Background (Slot 1) if a transition is active
            if (state.previousIndex != -1 && state.previousIndex != state.activeIndex) {
                VideoSource& prev = *state.sources[state.previousIndex];
                UpdateAndDraw(prev, 1.0f, 1); // Background is always fully opaque
            }

            // 2. Render Foreground (Slot 0)
            UpdateAndDraw(current, alpha, 0);

            renderer->SwapBuffers();
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

    // Helper to decode a frame and render it to a specific texture slot
    void UpdateAndDraw(VideoSource& source, float alphaValue, int slot) {
        bool frameReady = false;

        while (!frameReady) {
            int ret = av_read_frame(source.GetFmtCtx(), pkt); //

            if (ret >= 0) {
                if (pkt->stream_index == source.GetStreamIdx()) { //
                    avcodec_send_packet(source.GetCodecCtx(), pkt); //
                    if (avcodec_receive_frame(source.GetCodecCtx(), frm) >= 0) { //
                        // Standard upload and render logic
                        av_hwframe_transfer_data(sw_frm, frm, 0); //
                        renderer->UpdateVideoTextures(slot, sw_frm->width, sw_frm->height,
                            sw_frm->linesize[0], sw_frm->data[0],
                            sw_frm->linesize[1], sw_frm->data[1]); //

                        videoShader->Use(); //
                        glUniform1f(glGetUniformLocation(videoShader->programID, "uAlpha"), alphaValue); //
                        renderer->Render(videoShader->programID, slot); //

                        av_frame_unref(sw_frm); //
                        av_frame_unref(frm); //
                        frameReady = true;
                    }
                }
                av_packet_unref(pkt); //
            }
            else {
                // End of file reached: Drain decoder then Restart
                avcodec_send_packet(source.GetCodecCtx(), NULL); // Enter draining mode
                while (avcodec_receive_frame(source.GetCodecCtx(), frm) >= 0) {
                    // Optional: Render these last few frames to avoid a jump
                    av_frame_unref(frm);
                }
                // Restart without resetting the fade timer
                source.Restart(glfwGetTime(), false);
            }
        }
    }

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action != GLFW_PRESS) return;

        if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_LEFT) {
            int dir = (key == GLFW_KEY_RIGHT) ? 1 : -1;
            int nextIdx = (state.activeIndex + dir) % (int)state.sources.size();
            if (nextIdx < 0) nextIdx = (int)state.sources.size() - 1;

            // Trigger cross-fade transition logic
            state.previousIndex = state.activeIndex;
            state.activeIndex = nextIdx;

            // Manual switch: Reset the timer to trigger a fresh fade-in
            state.sources[state.activeIndex]->Restart(glfwGetTime(), true);
        }

        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
        if (key == GLFW_KEY_SPACE) state.sources[state.activeIndex]->TogglePause(glfwGetTime());
        if (key == GLFW_KEY_F && state.renderer) state.renderer->ToggleFullscreen();
    }
};