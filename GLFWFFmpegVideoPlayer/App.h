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
                // Apply separate fade timings
                videoSource->SetFadeInDuration(videoContent.fadeInDuration);
                videoSource->SetFadeOutDuration(videoContent.fadeOutDuration);
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

            // 1. Handle Background (Always index 0) with Frame-Rate Limiting
            VideoSource& background = *state.sources[0];
            if (!background.IsPaused()) {
                if (background.GetStartTime() <= 0) background.SetStartTime(glfwGetTime());

                double playPos = background.GetCurrentPlayPosition(glfwGetTime());

                // Only decode a new frame if the video time has advanced
                if (playPos > state.lastBackgroundPTS) {
                    UpdateAndDraw(background, 1.0f, 1);
                    state.lastBackgroundPTS = playPos;
                }
                else {
                    // Re-render existing texture to maintain VSync without stuttering
                    videoShader->Use();
                    glUniform1f(glGetUniformLocation(videoShader->programID, "uAlpha"), 1.0f);
                    renderer->Render(videoShader->programID, 1);
                }
            }

            // 2. Handle Foreground Interrupts
            if (state.activeIndex != 0) {
                VideoSource& foreground = *state.sources[state.activeIndex];

                if (foreground.GetStartTime() <= 0) foreground.SetStartTime(glfwGetTime());

                double currentTime = glfwGetTime();
                double elapsed = currentTime - foreground.GetAdjustedStartTime();
                double totalDuration = foreground.GetDurationInSeconds();

                float inDur = foreground.GetFadeInDuration();
                float outDur = foreground.GetFadeOutDuration();
                float alpha = 1.0f;

                // Fade In Logic
                if (elapsed < inDur && inDur > 0) {
                    alpha = (float)(elapsed / inDur);
                }
                // Fade Out Logic near the end of the file
                else if (elapsed > (totalDuration - outDur) && outDur > 0) {
                    double timeRemaining = totalDuration - elapsed;
                    alpha = (float)(timeRemaining / outDur);
                }

                if (alpha < 0.0f) alpha = 0.0f;
                if (alpha > 1.0f) alpha = 1.0f;

                UpdateAndDraw(foreground, alpha, 0); // Slot 0
            }

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

    void UpdateAndDraw(VideoSource& source, float alphaValue, int slot) {
        bool frameReady = false;
        while (!frameReady) {
            if (av_read_frame(source.GetFmtCtx(), pkt) >= 0) {
                if (pkt->stream_index == source.GetStreamIdx()) {
                    avcodec_send_packet(source.GetCodecCtx(), pkt);
                    if (avcodec_receive_frame(source.GetCodecCtx(), frm) >= 0) {
                        av_hwframe_transfer_data(sw_frm, frm, 0);

                        renderer->UpdateVideoTextures(slot,
                            sw_frm->width, sw_frm->height,
                            sw_frm->linesize[0], sw_frm->data[0],
                            sw_frm->linesize[1], sw_frm->data[1]
                        );

                        videoShader->Use();
                        glUniform1f(glGetUniformLocation(videoShader->programID, "uAlpha"), alphaValue);
                        renderer->Render(videoShader->programID, slot);

                        av_frame_unref(sw_frm);
                        av_frame_unref(frm);
                        frameReady = true;
                    }
                }
                av_packet_unref(pkt);
            }
            else {
                // End of stream handling
                if (state.activeIndex != 0 && &source == state.sources[state.activeIndex]) {
                    // Drain the decoder before switching back to background
                    avcodec_send_packet(source.GetCodecCtx(), NULL);
                    while (avcodec_receive_frame(source.GetCodecCtx(), frm) >= 0) { av_frame_unref(frm); }

                    state.activeIndex = 0; // Return to background view
                }
                else {
                    // Loop background without resetting fade timer
                    source.Restart(glfwGetTime(), false);
                    state.lastBackgroundPTS = -1.0; // Reset PTS tracker for smooth loop
                }
                frameReady = true;
            }
        }
    }

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action != GLFW_PRESS) return;

        if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_LEFT) {
            int dir = (key == GLFW_KEY_RIGHT) ? 1 : -1;
            int numSources = (int)state.sources.size();

            if (numSources <= 1) return;

            int nextIdx;
            if (state.lastForegroundIndex == 0) {
                nextIdx = 1; // Start with first foreground video
            }
            else {
                nextIdx = state.lastForegroundIndex + dir;
                if (nextIdx >= numSources) nextIdx = 1;
                if (nextIdx < 1) nextIdx = numSources - 1;
            }

            state.activeIndex = nextIdx;
            state.lastForegroundIndex = nextIdx;

            state.sources[state.activeIndex]->Restart(glfwGetTime(), true);
        }

        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
        if (key == GLFW_KEY_SPACE) state.sources[state.activeIndex]->TogglePause(glfwGetTime());
        if (key == GLFW_KEY_F && state.renderer) state.renderer->ToggleFullscreen();
    }
};