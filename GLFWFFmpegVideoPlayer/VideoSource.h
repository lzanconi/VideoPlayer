#pragma once
#include <string>
#include <iostream>
#include <windows.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
}

#include "GLRenderer.h"
#include "ShaderProgram.h"

class VideoSource
{
private:
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    int streamID = -1;
    double startTime = 0;
    double pauseTime = 0;
    double totalPausedTime = 0;
    double lastPTS = -1.0;

    std::string filename;
    bool isInitialized = false;
    bool isPaused = false;
    bool looped = false;

    float fadeInDuration = 2.5f;
    float fadeOutDuration = 1.0f;

public:
    VideoSource() = default;

    ~VideoSource()
    {
        Close();
    }

    bool Open(const std::string& file, AVBufferRef* hwDeviceCtx)
    {
        filename = file;
        if (avformat_open_input(&formatCtx, file.c_str(), NULL, NULL) < 0) {
            std::cerr << "Failed to open: " << file << std::endl;
            return false;
        }

        if (avformat_find_stream_info(formatCtx, NULL) < 0) return false;

        streamID = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (streamID < 0) return false;

        const AVCodec* decoder = avcodec_find_decoder(formatCtx->streams[streamID]->codecpar->codec_id);
        codecCtx = avcodec_alloc_context3(decoder);
        avcodec_parameters_to_context(codecCtx, formatCtx->streams[streamID]->codecpar);

        codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
        codecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
            for (const enum AVPixelFormat* p = pix_fmts; *p != -1; p++) {
                if (*p == AV_PIX_FMT_D3D11) return *p;
            }
            return AV_PIX_FMT_NONE;
            };

        if (avcodec_open2(codecCtx, decoder, NULL) < 0) return false;

        isInitialized = true;
        return true;
    }

    void Rewind()
    {
        if (!isInitialized) return;
        av_seek_frame(formatCtx, streamID, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codecCtx);
        lastPTS = -1.0;
    }

    void Play(double currentGLFWTime)
    {
        startTime = currentGLFWTime;
        totalPausedTime = 0;
        pauseTime = 0;
        isPaused = false;
        lastPTS = -1.0;
    }

    bool UpdateAndRender(GLRenderer* renderer, ShaderProgram* shader, AVFrame* frm, AVFrame* sw_frm, AVPacket* pkt, int slot)
    {
        if (!isInitialized) return true;

        double currentTime = glfwGetTime();

        if (startTime <= 0)
            return true;

        // UPDATED: If paused, calculate alpha based on the frozen pauseTime 
        // to stop the fade-in/out from progressing.
        if (isPaused) {
            shader->Use();
            glUniform1f(glGetUniformLocation(shader->programID, "uAlpha"), CalculateAlpha(pauseTime));
            renderer->Render(shader->programID, slot);
            return true;
        }

        double playPos = currentTime - GetAdjustedStartTime();
        float alpha = CalculateAlpha(currentTime);

        if (playPos > lastPTS) {
            bool frameReady = false;
            while (!frameReady) {
                if (av_read_frame(formatCtx, pkt) >= 0) {
                    if (pkt->stream_index == streamID) {
                        avcodec_send_packet(codecCtx, pkt);
                        if (avcodec_receive_frame(codecCtx, frm) >= 0) {
                            av_hwframe_transfer_data(sw_frm, frm, 0);

                            renderer->UpdateVideoTextures(slot,
                                sw_frm->width, sw_frm->height,
                                sw_frm->linesize[0], sw_frm->data[0],
                                sw_frm->linesize[1], sw_frm->data[1]
                            );

                            lastPTS = playPos;
                            frameReady = true;
                        }
                    }
                    av_packet_unref(pkt);
                }
                else {
                    if (looped) {
                        Rewind();
                        Play(glfwGetTime());
                        frameReady = true;
                    }
                    else {
                        return false;
                    }
                }
            }
        }

        shader->Use();
        glUniform1f(glGetUniformLocation(shader->programID, "uAlpha"), alpha);
        renderer->Render(shader->programID, slot);

        return true;
    }

    float CalculateAlpha(double currentTime)
    {
        // UPDATED: Use GetAdjustedStartTime() so 'elapsed' accounts for totalPausedTime.
        double elapsed = currentTime - GetAdjustedStartTime();
        double totalDuration = GetDurationInSeconds();
        float alpha = 1.0f;

        if (elapsed < fadeInDuration && fadeInDuration > 0) {
            alpha = (float)(elapsed / fadeInDuration);
        }
        else if (elapsed > (totalDuration - fadeOutDuration) && fadeOutDuration > 0) {
            double timeRemaining = totalDuration - elapsed;
            alpha = (float)(timeRemaining / fadeOutDuration);
        }

        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;
        return alpha;
    }

    void Pause(double currentGLFWTime)
    {
        if (!isInitialized) 
            return;
        
        if (!isPaused) {
            pauseTime = currentGLFWTime;
            isPaused = true;
        }
        else {
            totalPausedTime += (currentGLFWTime - pauseTime);
            isPaused = false;
        }
    }

    void Close()
    {
        if (codecCtx) avcodec_free_context(&codecCtx);
        if (formatCtx) avformat_close_input(&formatCtx);
        isInitialized = false;
    }

    double GetDurationInSeconds() const 
    {
        if (!formatCtx || streamID < 0) 
            return 0;

        return (double)formatCtx->streams[streamID]->duration * av_q2d(formatCtx->streams[streamID]->time_base);
    }

    void SetLooped(bool l) { looped = l; }

    double GetAdjustedStartTime() const { return startTime + totalPausedTime; }
    bool IsPaused() const { return isPaused; }
    void SetFadeInDuration(float d) { fadeInDuration = d; }
    void SetFadeOutDuration(float d) { fadeOutDuration = d; }
};