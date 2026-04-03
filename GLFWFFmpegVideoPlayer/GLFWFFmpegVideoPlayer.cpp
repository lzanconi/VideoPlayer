#include <iostream>
#include <windows.h>
#include <d3d11.h>

#include "GLRenderer.h"
#include "ShaderProgram.h"
#include "VideoSource.h"

// --- Global Application State ---
struct AppState {
    int activeIndex = 0;
    bool interruptRead = false;
    VideoSource sources[2];
    GLRenderer* renderer = nullptr;
} state;

// --- Keyboard Interaction ---
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) 
        return;

    if (key == GLFW_KEY_F) {
        if (state.renderer) {
            state.renderer->ToggleFullscreen();
        }
    }

    if (key == GLFW_KEY_ESCAPE) 
        glfwSetWindowShouldClose(window, true);

    // Switch and Restart Logic
    if (key == GLFW_KEY_1 || key == GLFW_KEY_2) {
        int newIdx = (key == GLFW_KEY_1) ? 0 : 1;

        state.activeIndex = newIdx;
        state.interruptRead = true; // Signal the decoding loop to stop immediately
        state.sources[state.activeIndex].Restart(glfwGetTime());

        std::cout << "Switched to Video " << (state.activeIndex + 1) << " and Restarted." << std::endl;
    }
}

int main() {
    // 1. Initialize Window (OpenGL Context + Quad Geometry + Textures)
    GLRenderer renderer(1280, 720, "Video Switcher Pro");
    renderer.SetKeyCallback(key_callback);
    state.renderer = &renderer;

    // 2. Load Shaders
    ShaderProgram videoShader("shader.vert", "shader.frag");
    videoShader.SetTextureUnits();

    // 3. Setup Hardware Acceleration (D3D11VA)
    AVBufferRef* hw_ctx = NULL;
    if (av_hwdevice_ctx_create(&hw_ctx, AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, 0) < 0) {
        std::cerr << "Failed to create HW Device Context" << std::endl;
        return -1;
    }

    // 4. Open Video Sources
    if (!state.sources[0].Open("test.mp4", hw_ctx)) return -1;
    if (!state.sources[1].Open("cover1.mp4", hw_ctx)) return -1;

    // 5. Decoding Buffers
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frm = av_frame_alloc();
    AVFrame* sw_frm = av_frame_alloc();

    // 6. Main Execution Loop
    while (!renderer.ShouldClose()) {
        state.interruptRead = false; // Reset the interrupt flag
        VideoSource& current = state.sources[state.activeIndex];

        // Ensure current video has an active start time for sync
        if (current.GetStartTime() <= 0) {
            current.SetStartTime(glfwGetTime());
        }

        // Read packet from file
        if (av_read_frame(current.GetFmtCtx(), pkt) >= 0) {
            if (pkt->stream_index == current.GetStreamIdx()) {
                avcodec_send_packet(current.GetCodecCtx(), pkt);

                // Receive decoded frames from GPU
                while (avcodec_receive_frame(current.GetCodecCtx(), frm) >= 0) {

                    // Break immediately if user switched videos
                    if (state.interruptRead) {
                        av_frame_unref(frm);
                        break;
                    }

                    // --- Synchronization ---
                    double pts = frm->pts * av_q2d(current.GetFmtCtx()->streams[current.GetStreamIdx()]->time_base);
                    double elapsed = glfwGetTime() - current.GetStartTime();
                    if (pts > elapsed) {
                        if (pts - elapsed > 0.002) Sleep((DWORD)((pts - elapsed) * 1000));
                    }

                    // Re-check interrupt after sleep
                    if (!state.interruptRead) {
                        // Move frame from GPU memory to System RAM (NV12 format)
                        av_hwframe_transfer_data(sw_frm, frm, 0);

                        // Upload to textures and render using our GLWindow class
                        renderer.UpdateVideoTextures(
                            sw_frm->width, sw_frm->height,
                            sw_frm->linesize[0], sw_frm->data[0],
                            sw_frm->linesize[1], sw_frm->data[1]
                        );

                        renderer.Render(videoShader.programID);
                        renderer.SwapBuffers();
                    }

                    av_frame_unref(sw_frm);
                    av_frame_unref(frm);
                    renderer.PollEvents();
                }
            }
            av_packet_unref(pkt);
        }
        else {
            // Auto-Restart on End of File
            current.Restart(glfwGetTime());
        }

        renderer.PollEvents();
    }

    // 7. Cleanup
    // Classes (renderer, shader, sources) clean themselves up via destructors (RAII)
    av_frame_free(&frm);
    av_frame_free(&sw_frm);
    av_packet_free(&pkt);
    if (hw_ctx) av_buffer_unref(&hw_ctx);

    return 0;
}