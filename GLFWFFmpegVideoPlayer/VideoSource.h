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
	// Cumulative time spent paused
	double totalPausedTime = 0;
	std::string filename;
	bool isInitialized = false;
	bool isPaused = false;

	// Separate properties for fade-in and fade-out
	float fadeInDuration = 2.5f;
	float fadeOutDuration = 2.5f;

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
		codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
		codecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
			for (const enum AVPixelFormat* p = pix_fmts; *p != -1; p++) {
				// Corrected from AV_PI_FMT_D3D11 to AV_PIX_FMT_D3D11
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

	void Restart(double currentGLFWTime, bool resetTimer = true)
	{
		if (!isInitialized)
			return;

		av_seek_frame(formatCtx, streamID, 0, AVSEEK_FLAG_BACKWARD);
		avcodec_flush_buffers(codecCtx);

		if (resetTimer)
		{
			startTime = currentGLFWTime;
			totalPausedTime = 0;
		}

		pauseTime = 0;
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

	// Helper to calculate total duration in seconds for fade-out timing
	double GetDurationInSeconds() const {
		if (!formatCtx || streamID < 0) return 0;
		// Duration is stored in time_base units
		return (double)formatCtx->streams[streamID]->duration * av_q2d(formatCtx->streams[streamID]->time_base);
	}

	// Getters and Setters
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

	float GetFadeInDuration() const { return fadeInDuration; }
	void SetFadeInDuration(float duration) { fadeInDuration = duration; }
	float GetFadeOutDuration() const { return fadeOutDuration; }
	void SetFadeOutDuration(float duration) { fadeOutDuration = duration; }
};