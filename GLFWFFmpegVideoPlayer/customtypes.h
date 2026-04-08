#pragma once
#include <vector>

class VideoSource;
class IRenderer;
class NetworkManager;

struct AppState
{
    int activeIndex = 0;
    int previousIndex = -1;
    int lastForegroundIndex = 0;
    bool interruptRead = false;
    std::vector<VideoSource*> sources;
    IRenderer* renderer = nullptr;
    NetworkManager* networkMgr = nullptr;
    double lastBackgroundPTS = -1.0;

    // FPS Tracking
    double lastFPSUpdate = 0;
    int frameCount = 0;
};

struct VideoContent
{
    std::string filename;
    float fadeInDuration = 2.5f;
    float fadeOutDuration = 1.0f;
    bool looped = false;
    std::vector<float> positions;
};