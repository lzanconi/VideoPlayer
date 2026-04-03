#pragma once
#include "VideoSource.h"
#include "GLRenderer.h"

struct AppState 
{
    int activeIndex = 0;
    bool interruptRead = false;
    std::vector<VideoSource*> sources;
    GLRenderer* renderer = nullptr;
};

struct VideoContent
{
    std::string filename;
};