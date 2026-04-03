#pragma once
#include "VideoSource.h"
#include "GLRenderer.h"

struct AppState {
    int activeIndex = 0;
    bool interruptRead = false;
    VideoSource sources[2];
    GLRenderer* renderer = nullptr;
};