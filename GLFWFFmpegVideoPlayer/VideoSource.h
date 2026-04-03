#pragma once
#include <string>
#include <iostream>
#include <windows.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
}

class VideoSource
{
private:
	AVFormatContext* formatCtx = nullptr;
	AVCodecContext* codecCtx = nullptr;
	int streamID = -1;
	double startTime = 0;
	// Time when the pause started
	double pauseTime = 0;
	//Cumulative time spent paused
	double totalPausedTime = 0;
	std::string filename;
	bool isInitialized = false;
	bool isPaused = false;
	float fadeDuration = 0.0f;

public:
	VideoSource() = default;

	~VideoSource()
	{
		Close();
	}

	bool Open(const std::string& file, AVBufferRef* hwDeviceCtx)
	{
		filename = file;

		// 1. Open File
		if (avformat_open_input(&formatCtx, file.c_str(), NULL, NULL) < 0) {
			std::cerr << "Failed to open: " << file << std::endl;
			return false;
		}

		// 2. Get Stream Info
		if (avformat_find_stream_info(formatCtx, NULL) < 0) return false;

		// 3. Find Video Stream
		streamID = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
		if (streamID < 0) return false;

		// 4. Setup Decoder
		const AVCodec* decoder = avcodec_find_decoder(formatCtx->streams[streamID]->codecpar->codec_id);
		codecCtx = avcodec_alloc_context3(decoder);
		avcodec_parameters_to_context(codecCtx, formatCtx->streams[streamID]->codecpar);

		// 5. Link Hardware Context
		codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
		codecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
			for (const enum AVPixelFormat* p = pix_fmts; *p != -1; p++) {
				if (*p == AV_PIX_FMT_D3D11) return *p;
			}
			return AV_PIX_FMT_NONE;
			};

		if (avcodec_open2(codecCtx, decoder, NULL) < 0) return false;

		startTime = 0; // Will be set on first frame read
		isInitialized = true;
		return true;
	}

	void TogglePause(double currentGLFWTime)
	{
		if (!isInitialized)
			return;

		if (!isPaused)
		{
			pauseTime = currentGLFWTime;
			isPaused = true;
		}
		else
		{
			totalPausedTime += (currentGLFWTime - pauseTime);
			isPaused = false;
		}
	}

	// Seek to beginning and reset timing
	void Restart(double currentGLFWTime)
	{
		if (!isInitialized)
			return;

		// Seek to the very beginning of the video stream
		av_seek_frame(formatCtx, streamID, 0, AVSEEK_FLAG_BACKWARD);

		// CRITICAL: Tell the hardware decoder to forget any frames it's currently processing
		avcodec_flush_buffers(codecCtx);

		// Reset our synchronization anchor
		startTime = currentGLFWTime;
		//Reset pause anchor
		pauseTime = 0;
		//Reset cumulative pause
		totalPausedTime = 0;
		//Ensure it starts playing
		isPaused = false;
	}

	void Close() 
	{
		if (codecCtx) 
			avcodec_free_context(&codecCtx);

		if (formatCtx) 
			avformat_close_input(&formatCtx);

		isInitialized = false;
	}

	// Getters
	AVFormatContext* GetFmtCtx() { return formatCtx; }
	AVCodecContext* GetCodecCtx() { return codecCtx; }
	int GetStreamIdx() const { return streamID; }
	double GetStartTime() const { return startTime; }
	void SetStartTime(double t) { startTime = t; }
	std::string GetFilename() const { return filename; }
	bool Initialized() const { return isInitialized; }
	bool IsPaused() const { return isPaused; }
	double GetAdjustedStartTime() const {
		return startTime + totalPausedTime;
	}
	float GetFadeDuration() const { return fadeDuration; }
	void SetFadeDuration(float duration) { fadeDuration = duration; }
};