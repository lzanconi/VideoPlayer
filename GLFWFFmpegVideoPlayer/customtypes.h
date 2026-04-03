#pragma once
#include "VideoSource.h"
#include "GLRenderer.h"

struct AppState
{
    int activeIndex = 0;
    int previousIndex = -1;
    int lastForegroundIndex = 0;
    bool interruptRead = false;
    std::vector<VideoSource*> sources;
    GLRenderer* renderer = nullptr;
    double lastBackgroundPTS = -1.0;
};

struct VideoContent
{
    std::string filename;
    float fadeInDuration = 2.5f;
    float fadeOutDuration = 1.0f;
};