#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <cstdint>

// Forward declarations for FFmpeg structs to keep the header clean
struct AVFormatContext;
struct AVCodecContext;
struct AVBufferRef;
struct AVFrame;
struct AVPacket;

class IRenderer;
class ShaderProgram;

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

    VideoSource();
    ~VideoSource();

    // Initializes FFmpeg contexts and hardware decoding
    bool Open(const std::string& file, AVBufferRef* hwDeviceCtx);

    // Resets the video to the first frame
    void Rewind();

    // Starts or resumes playback
    void Play(double currentGLFWTime);

    // Main update loop: decodes and renders frames
    bool UpdateAndRender(IRenderer* renderer, ShaderProgram* shader, AVFrame* frm, AVPacket* pkt, int slot);

    // Toggles pause state
    void Pause(double currentGLFWTime);

    // Cleans up FFmpeg resources
    void Close();

    // Utility getters and setters
    double GetDurationInSeconds() const;
    void SetLooped(bool l);
    double GetLastPTS();
    double GetAdjustedStartTime() const;
    int64_t GetBGCaptureTimeNS();
    bool IsPaused() const;
    void SetFadeInDuration(float d);
    void SetFadeOutDuration(float d);

private:
    // Calculates transparency for cross-fading
    float CalculateAlpha(double currentTime);
};