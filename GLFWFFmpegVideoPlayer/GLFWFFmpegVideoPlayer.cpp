#include <iostream>
#include <windows.h>
#include <d3d11.h>

#include "customtypes.h"
#include "GLRenderer.h"
#include "ShaderProgram.h"
#include "VideoSource.h"
#include "App.h"

AppState App::state;

int main()
{
	try
	{
		std::vector<VideoContent> playlist = {
			{"full_test.mp4", 2.0f, 1.0f},
			{"1.mp4", 2.0f, 1.0f},
			{"2.mp4", 2.0f, 1.0f},
			{"3.mp4", 2.0f, 0.0f}
		};

		App app(1280, 720, "Video Player", playlist);
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << "Application Error: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}