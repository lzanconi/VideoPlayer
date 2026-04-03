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
        // 1. Initialize Global State and Callbacks
        renderer = new GLRenderer(width, height, title.c_str());
        renderer->SetKeyCallback(App::KeyCallback);
        state.renderer = renderer;

        // 2. Load Shaders and Setup Texture Units
        videoShader = new ShaderProgram("shader.vert", "shader.frag");
        videoShader->SetTextureUnits();

        // 3. Setup Hardware Acceleration (D3D11VA)
        if (av_hwdevice_ctx_create(&hw_ctx, AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, 0) < 0) {
            throw std::runtime_error("Failed to create HW Device Context");
        }

        for (const auto& videoContent : videoContents)
        {
            VideoSource* videoSource = new VideoSource();
            if (videoSource->Open(videoContent.filename, hw_ctx))
            {
                state.sources.push_back(videoSource);
            }
            else
            {
                delete videoSource;
                std::cerr << "Warning: Could not open " << videoContent.filename << std::endl;
            }
        }

        if (state.sources.empty()) 
            throw std::runtime_error("No videos could be loaded");

        // 5. Allocate Decoding Buffers
        pkt = av_packet_alloc();
        frm = av_frame_alloc();
        sw_frm = av_frame_alloc();
	}

    ~App()
    {
        if (renderer)
            delete renderer;

        if (videoShader)
            delete videoShader;

        for (auto source : state.sources)
        {
            delete source;
        }
        state.sources.clear();

        av_frame_free(&frm);
        av_frame_free(&sw_frm);
        av_packet_free(&pkt);

        if (hw_ctx) 
            av_buffer_unref(&hw_ctx);
    }

    void Run()
    {
        // 6. Main Execution Loop
        while (!renderer->ShouldClose()) {
            state.interruptRead = false;
            VideoSource& current = *state.sources[state.activeIndex];

            // Sync check
            if (current.GetStartTime() <= 0) {
                current.SetStartTime(glfwGetTime());
            }

            if (av_read_frame(current.GetFmtCtx(), pkt) >= 0) {
                if (pkt->stream_index == current.GetStreamIdx()) {
                    avcodec_send_packet(current.GetCodecCtx(), pkt);

                    while (avcodec_receive_frame(current.GetCodecCtx(), frm) >= 0) {
                        if (state.interruptRead) {
                            av_frame_unref(frm);
                            break;
                        }

                        // Synchronization logic
                        double pts = frm->pts * av_q2d(current.GetFmtCtx()->streams[current.GetStreamIdx()]->time_base);
                        double elapsed = glfwGetTime() - current.GetStartTime();
                        if (pts > elapsed) {
                            if (pts - elapsed > 0.002) Sleep((DWORD)((pts - elapsed) * 1000));
                        }

                        if (!state.interruptRead) {
                            // Transfer GPU data to System RAM and Render
                            av_hwframe_transfer_data(sw_frm, frm, 0);
                            renderer->UpdateVideoTextures(
                                sw_frm->width, sw_frm->height,
                                sw_frm->linesize[0], sw_frm->data[0],
                                sw_frm->linesize[1], sw_frm->data[1]
                            );

                            renderer->Render(videoShader->programID);
                            renderer->SwapBuffers();
                        }

                        av_frame_unref(sw_frm);
                        av_frame_unref(frm);
                        renderer->PollEvents();
                    }
                }
                av_packet_unref(pkt);
            }
            else {
                current.Restart(glfwGetTime());
            }
            renderer->PollEvents();
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

    // Static callback required for GLFW compatibility
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action != GLFW_PRESS) return;

        if (key == GLFW_KEY_F && state.renderer) {
            state.renderer->ToggleFullscreen();
        }

        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(window, true);

        if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_LEFT) 
        {
            int dir = (key == GLFW_KEY_RIGHT) ? 1 : -1;
            int nextIdx = (state.activeIndex + dir) % (int)state.sources.size();
            if (nextIdx < 0) nextIdx = (int)state.sources.size() - 1;

            state.activeIndex = nextIdx;
            state.interruptRead = true;
            state.sources[state.activeIndex]->Restart(glfwGetTime());
            std::cout << "Switched to: " << state.sources[state.activeIndex]->GetFilename() << std::endl;
        }
    }

};