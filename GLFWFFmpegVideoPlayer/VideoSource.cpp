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

VideoSource::~VideoSource() 
{
    Close();
}

/*
* Responsible for opening the video file, initializing the FFmpeg format and codec contexts, and setting up hardware acceleration
*/
bool VideoSource::Open(const std::string& file, AVBufferRef* hwDeviceCtx) 
{
    filename = file;
    //Opens the video file and reads the header to understand the container format
    if (avformat_open_input(&formatCtx, file.c_str(), NULL, NULL) < 0) 
    {
        std::cerr << "Failed to open: " << file << std::endl;
        return false;
    }

    //Analyzes the file to get detailed information about the streams (video, audio, etc.)
    if (avformat_find_stream_info(formatCtx, NULL) < 0)
        return false;

    //Specifically searches for the primary video stream within the file and returns its index
    streamID = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamID < 0)
        return false;

    //Looks up the appropriate decoder (like H.264, HEVC, VP9 etc.) based on the video's codec ID
    const AVCodec* decoder = avcodec_find_decoder(formatCtx->streams[streamID]->codecpar->codec_id);
    //Creates a codec context which holds the settings and state for the decoding process
    codecCtx = avcodec_alloc_context3(decoder);
    //Copies the settings from the file (like resolution and framerate) into the decoder context
    avcodec_parameters_to_context(codecCtx, formatCtx->streams[streamID]->codecpar);

    //Receives a reference to a hardware device context (created in App.h) and assigns it to the codec
    codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);

    //Tells FFmpeg to pick AV_PIX_FMT_D3D11 format if it can to ensure the decoding happens directly on the GPU rather than the CPU
    codecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) 
    {
        for (const enum AVPixelFormat* p = pix_fmts; *p != -1; p++) 
        {
            if (*p == AV_PIX_FMT_D3D11) return *p;
        }
        
        return AV_PIX_FMT_NONE;
    };

    //Opens the decoder with the configured settings
    if (avcodec_open2(codecCtx, decoder, NULL) < 0)
        return false;

    //Sets isInitialized to true allowing UpdateAndRender() to start processing frames
    isInitialized = true;
    return true;
}

/*
* It is the central "engine" of each video stream.
* It manages playback timing, handles the decoding of raw packets into frames, and triggers the final render for a specific video layer.
*/
bool VideoSource::UpdateAndRender(IRenderer* renderer, ShaderProgram* shader, AVFrame* gpu_frame, AVFrame* cpu_frame, AVPacket* raw_packet, int slot) 
{
    //Check if the video source has been successfully opened
    if (!isInitialized) 
        return true;
    
    //Retrieves the current time from the GLFW library to synchronize playback later
    double currentTime = glfwGetTime();

    //Ensures the video has been assigned a start time via the Play() method before proceeding
    if (startTime <= 0) 
        return true;

    //PAUSED
    //If the video is paused, it stops processing new frames and show the last rendered frame and the 
    //same alpha that was computed when paused was triggered 
    if (isPaused) 
    {
        shader->Use();
        glUniform1f(glGetUniformLocation(shader->programID, "uAlpha"), CalculateAlpha(pauseTime));
        renderer->Render(shader->programID, slot);
        
        return true;
    }

    //"playPos" determines where we are in the video stream by subtracting the start time 
    //(and any accumulated pause duration) from the current time
    double playPos = currentTime - GetAdjustedStartTime();

    //Compute the current fade-in or fade-out transparency level
    float alpha = CalculateAlpha(currentTime);

    //If the current playback time "playPos" is ahead of the last displayed frame's timestamp (lastPTS),
    //it's time to decode a new frame
    if (playPos > lastPTS) 
    {
        //Ensures that the application successfully retrieves all the necessary data to 
        //produce a complete frame
        bool frameCompleted = false;
        
        while (!frameCompleted) 
        {
            //Fetches raw packet (compressed data) from FFmpeg format context
            if (av_read_frame(formatCtx, raw_packet) >= 0) 
            {
                //Ensures the raw packet belongs to the video stream
                if (raw_packet->stream_index == streamID) 
                {
                    //Send the compressed raw packet to the FFmpeg codec context for decompression
                    avcodec_send_packet(codecCtx, raw_packet);

                    //Attempts to pull a decoded YUV frame from the decoder
                    if (avcodec_receive_frame(codecCtx, gpu_frame) >= 0)
                    {
                        //Copy the frame from the GPU hardware decoder memory (gpu_frame) to software-accessible buffer (cpu_frame)
                        av_hwframe_transfer_data(cpu_frame, gpu_frame, 0);

                        //Sends the raw Y (luminance) and UV (chrominance) data to the OpenGL renderer to update
                        //the textures for the specific slot
                        renderer->UpdateVideoTextures(slot,
                            cpu_frame->width, cpu_frame->height,
                            cpu_frame->linesize[0], cpu_frame->data[0],
                            cpu_frame->linesize[1], cpu_frame->data[1]
                        );

                        //Updates "lastPts" to the current position and exits the loop because the current frame 
                        //has been completed and it's ready to be rendered by the "Render()" method 
                        lastPTS = playPos;
                        frameCompleted = true;
                    }
                }
                av_packet_unref(raw_packet);
            }
            //LOOP THE VIDEO
            //The video has reached its end, check if has to loop
            else 
            {
                if (looped) 
                {
                    Rewind();
                    Play(glfwGetTime());
                    frameCompleted = true;
                }
                else 
                {
                    return false;
                }
            }
        }
    }

    //Records the exact high-resolution timestamp of when this frame was captured for network sync
    bg_capture_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    //RENDER THE FRAME
    //Finally render the frame using the computed alpha
    shader->Use();
    glUniform1f(glGetUniformLocation(shader->programID, "uAlpha"), alpha);
    renderer->Render(shader->programID, slot);
    return true;
}

/*
* Responsible for calculating the transparency (alpha) of a video frame based on its current playback position, 
* enabling smooth cross-fading transitions.
*/
float VideoSource::CalculateAlpha(double currentTime) 
{
    double elapsed = currentTime - GetAdjustedStartTime();
    double totalDuration = GetDurationInSeconds();
    float alpha = 1.0f;

    if (elapsed < fadeInDuration && fadeInDuration > 0) 
    {
        alpha = (float)(elapsed / fadeInDuration);
    }
    else if (elapsed > (totalDuration - fadeOutDuration) && fadeOutDuration > 0) 
    {
        double timeRemaining = totalDuration - elapsed;
        alpha = (float)(timeRemaining / fadeOutDuration);
    }

    if (alpha < 0.0f) 
        alpha = 0.0f;
    
    if (alpha > 1.0f) 
        alpha = 1.0f;
    
    return alpha;
}

/*
* Rewinds a video to its very first frame.
*/
void VideoSource::Rewind()
{
    if (!isInitialized)
        return;

    //AVSEEK_FLAG_BACKWARD tells the decoder to find the nearest keyframe (a complete image) at or 
    //before the timestamp of 0
    av_seek_frame(formatCtx, streamID, 0, AVSEEK_FLAG_BACKWARD);

    //Because video decoders often "look ahead" and store several frames in memory to handle compression,
    //simply moving the file pointer is not enough
    //Flushing clears out any leftover data from the previous playback position, preventing the player from 
    //accidentally showing "ghost frames" from the end of the video when it starts over
    avcodec_flush_buffers(codecCtx);

    //The lastPTS variable is used by the UpdateAndRender function to track the timing of the last displayed frame.
    //Resetting this value synchronizes the internal logic, signaling to the application that it is now ready to process 
    //a brand-new sequence of frames starting from time zero.
    lastPTS = -1.0;
}

void VideoSource::Play(double currentGLFWTime)
{
    startTime = currentGLFWTime;
    totalPausedTime = 0;
    pauseTime = 0;
    isPaused = false;
    lastPTS = -1.0;
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