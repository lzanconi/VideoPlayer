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

#include "IRenderer.h"
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
    std::atomic<int64_t> bg_capture_time_ns;
    

public:
    std::vector<float> positions;

    VideoSource() = default;

    ~VideoSource()
    {
        Close();
    }

    /*
	* Responsible for opening the video file, initializing the FFmpeg format and codec contexts, and setting up hardware acceleration
    */
    bool Open(const std::string& file, AVBufferRef* hwDeviceCtx)
    {
        filename = file;
        //Opens the video and reads the header to understand the container format
        if (avformat_open_input(&formatCtx, file.c_str(), NULL, NULL) < 0) {
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

		//Looks up the appropriate decoder for (like H.264, HEVC, VP9 etc.) based on the video's codec ID
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
    * Rewinds a video to its very first frame. 
    */
    void Rewind()
    {
        if (!isInitialized) 
            return;

        //AVSEEK_FLAG_BACKWARD tells the decoder to find the nearest keyframe (a complete image) at or 
        //before the timestamp of 0
        av_seek_frame(formatCtx, streamID, 0, AVSEEK_FLAG_BACKWARD);
        //Because video decoders often "look ahead" and store several frames in memory to handle compression,
        //simply moving the file pointer is not enough.
        //Flushing clears out any leftover data from the previous playback position, preventing the player from 
		//accidentally showing "ghost frames" from the end of the video when it starts over.
        avcodec_flush_buffers(codecCtx);

        //The lastPTS variable is used by the UpdateAndRender function to track the timing of the last displayed frame. 
        //Resetting this value synchronizes the internal logic, signaling to the application that it is now ready to process 
        //a brand-new sequence of frames starting from time zero.
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

    /*
    * It is the central "engine" of each video stream. 
    * It manages playback timing, handles the decoding of raw packets into frames, and triggers the final render for a specific video layer.
    */
    bool UpdateAndRender(IRenderer* renderer, ShaderProgram* shader, AVFrame* frm, AVFrame* sw_frm, AVPacket* pkt, int slot)
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

            //FRAME DECODING LOOP
            //While a frame is not yet completed...
            while (!frameCompleted) 
            {
                //Fetches raw packet (compressed data) from FFmpeg format context
                if (av_read_frame(formatCtx, pkt) >= 0) 
                {
                    //Ensures the packet belongs to the video stream
                    if (pkt->stream_index == streamID) 
                    {
                        //Send the compressed packet to the FFmpeg codec context for decompression
                        avcodec_send_packet(codecCtx, pkt);

                        //Attempts to pull a decoded YUV frame from the decoder
                        if (avcodec_receive_frame(codecCtx, frm) >= 0) 
                        {
                            //Copy the frame from the GPU hardware decoder memory to software-accessible buffer (sw_frm)
                            av_hwframe_transfer_data(sw_frm, frm, 0);

                            //Sends the raw Y (luminance) and UV (chrominance) data to the OpenGL renderer to update
                            //the textures for the specific slot
                            renderer->UpdateVideoTextures(slot,
                                sw_frm->width, sw_frm->height,
                                sw_frm->linesize[0], sw_frm->data[0],
                                sw_frm->linesize[1], sw_frm->data[1]
                            );

                            //Updates "lastPts" to the current position and exits the loop because the current frame 
                            //has been completed and it's ready to be rendered by the "Render()" method 
                            lastPTS = playPos;
                            frameCompleted = true;
                        }
                    }
                    av_packet_unref(pkt);
                }
                //LOOP
                //The video has reached its end, check if has to loop
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

        //RENDER THE FRAME
        //Finally render the frame using the computed alpha
        bg_capture_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count(); 

        shader->Use();
        glUniform1f(glGetUniformLocation(shader->programID, "uAlpha"), alpha);
        renderer->Render(shader->programID, slot);

        return true;
    }

    float CalculateAlpha(double currentTime)
    {
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
    double GetLastPTS() { return lastPTS;  }
    double GetAdjustedStartTime() const { return startTime + totalPausedTime; }
    int64_t GetBGCaptureTimeNS() { return bg_capture_time_ns;  }

    bool IsPaused() const { return isPaused; }
    void SetFadeInDuration(float d) { fadeInDuration = d; }
    void SetFadeOutDuration(float d) { fadeOutDuration = d; }
};