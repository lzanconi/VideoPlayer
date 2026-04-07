#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include "customtypes.h"

namespace fs = std::filesystem;

class ContentManager
{
public:
	ContentManager() = default;

    void LoadFromFolder(const std::string& folderPath) {
        videoContents.clear();

        try {
            if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
                std::cerr << "Directory does not exist: " << folderPath << std::endl;
                return;
            }

            for (const auto& entry : fs::directory_iterator(folderPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".mp4") {
                    VideoContent content;
                    content.filename = entry.path().string();

                    // Set default behaviors
                    content.fadeInDuration = 1.0f;
                    content.fadeOutDuration = 1.0f;
                    content.looped = false;

                    videoContents.push_back(content);
                }
            }

            // 1. Find the first video that contains "bg" in its filename
            auto it = std::find_if(videoContents.begin(), videoContents.end(), [](const VideoContent& v) {
                return v.filename.find("bg") != std::string::npos;
                });

            // 2. If found, move it to the first position
            if (it != videoContents.end()) {
                std::rotate(videoContents.begin(), it, it + 1);
            }

            std::cout << "ContentManager: Loaded " << videoContents.size() << " videos." << std::endl;
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
        }
    }

	const std::vector<VideoContent>& GetVideoContents() const 
	{
		return videoContents;
	}

private:
	std::vector<VideoContent> videoContents;
};