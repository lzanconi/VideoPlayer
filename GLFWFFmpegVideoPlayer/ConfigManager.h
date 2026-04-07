#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "customtypes.h"
#include <algorithm>

class ConfigManager
{
public:
	ConfigManager(const std::string& configFilePath)
	{
        LoadFromFile(configFilePath);
	}

	const std::vector<VideoContent>& GetVideoContents() const
	{
		return videoContents;
	}

private:
	std::vector<VideoContent> videoContents;

    void LoadFromFile(const std::string& configPath) 
    {
        std::ifstream configFile(configPath);

        if (!configFile.is_open()) {
            std::cerr << "ConfigManager Error: Could not open config file: " << configPath << std::endl;
            return;
        }

        std::string line;
        while (std::getline(configFile, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::stringstream ss(line);
            VideoContent content;
            std::string loopStr;

            // Parse filename, durations, and then the loop status as a string
            if (ss >> content.filename >> content.fadeInDuration >> content.fadeOutDuration >> loopStr) {
                // Convert string to lowercase to handle "True", "TRUE", etc.
                std::transform(loopStr.begin(), loopStr.end(), loopStr.begin(), ::tolower);

                content.looped = (loopStr == "true");
                videoContents.push_back(content);
            }
        }
        configFile.close();
    }

};