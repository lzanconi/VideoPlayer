#include <iostream>
#include <windows.h>
#include <d3d11.h>

#include "App.h"
#include "ContentManager.h"

int main()
{
	try
	{
		/*std::vector<VideoContent> videoContents = {
			{"full_test.mp4", 0.0f, 0.0f, true},
			{"1.mp4", 0.0f, 0.0f, true},
			{"2.mp4", 2.0f, 1.0f, false},
			{"3.mp4", 2.0f, 0.0f, false}
		};*/

		// Initialize the App with window dimensions and title
		App app(1280, 720, "Video Player");
		// Start the application's main loop
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << "Application Error: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}