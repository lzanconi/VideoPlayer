#include "VideoSource.h"
#include "IRenderer.h"
#include "ShaderProgram.h"
#include <iostream>
#include <chrono>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
}

VideoSource::VideoSource() : bg_capture_time_ns(0) {}

VideoSource::~VideoSource() {
    Close();
}

bool VideoSource::Open(const std::string& file, AVBufferRef* hwDeviceCtx) {
    filename = file;
    if (avformat_open_input(&formatCtx, file.c_str(), NULL, NULL) < 0) {
        std::cerr << "Failed to open: " << file << std::endl;
        return false;
    }

    if (avformat_find_stream_info(formatCtx, NULL) < 0)
        return false;

    streamID = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamID < 0)
        return false;

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

    if (avcodec_open2(codecCtx, decoder, NULL) < 0)
        return false;

    isInitialized = true;
    return true;
}

void VideoSource::Rewind() {
    if (!isInitialized) return;
    av_seek_frame(formatCtx, streamID, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCtx);
    lastPTS = -1.0;
}

void VideoSource::Play(double currentGLFWTime) {
    startTime = currentGLFWTime;
    totalPausedTime = 0;
    pauseTime = 0;
    isPaused = false;
    lastPTS = -1.0;
}

bool VideoSource::UpdateAndRender(IRenderer* renderer, ShaderProgram* shader, AVFrame* frm, AVFrame* sw_frm, AVPacket* pkt, int slot) {
    if (!isInitialized) return true;
    double currentTime = glfwGetTime();
    if (startTime <= 0) return true;

    if (isPaused) {
        shader->Use();
        glUniform1f(glGetUniformLocation(shader->programID, "uAlpha"), CalculateAlpha(pauseTime));
        renderer->Render(shader->programID, slot);
        return true;
    }

    double playPos = currentTime - GetAdjustedStartTime();
    float alpha = CalculateAlpha(currentTime);

    if (playPos > lastPTS) {
        bool frameCompleted = false;
        while (!frameCompleted) {
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
                        frameCompleted = true;
                    }
                }
                av_packet_unref(pkt);
            }
            else {
                if (looped) {
                    Rewind();
                    Play(glfwGetTime());
                    frameCompleted = true;
                }
                else {
                    return false;
                }
            }
        }
    }

    bg_capture_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    shader->Use();
    glUniform1f(glGetUniformLocation(shader->programID, "uAlpha"), alpha);
    renderer->Render(shader->programID, slot);
    return true;
}

float VideoSource::CalculateAlpha(double currentTime) {
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

void VideoSource::Pause(double currentGLFWTime) {
    if (!isInitialized) return;
    if (!isPaused) {
        pauseTime = currentGLFWTime;
        isPaused = true;
    }
    else {
        totalPausedTime += (currentGLFWTime - pauseTime);
        isPaused = false;
    }
}

void VideoSource::Close() {
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (formatCtx) avformat_close_input(&formatCtx);
    isInitialized = false;
}

double VideoSource::GetDurationInSeconds() const {
    if (!formatCtx || streamID < 0) return 0;
    return (double)formatCtx->streams[streamID]->duration * av_q2d(formatCtx->streams[streamID]->time_base);
}

void VideoSource::SetLooped(bool l) { looped = l; }
double VideoSource::GetLastPTS() { return lastPTS; }
double VideoSource::GetAdjustedStartTime() const { return startTime + totalPausedTime; }
int64_t VideoSource::GetBGCaptureTimeNS() { return bg_capture_time_ns; }
bool VideoSource::IsPaused() const { return isPaused; }
void VideoSource::SetFadeInDuration(float d) { fadeInDuration = d; }
void VideoSource::SetFadeOutDuration(float d) { fadeOutDuration = d; }