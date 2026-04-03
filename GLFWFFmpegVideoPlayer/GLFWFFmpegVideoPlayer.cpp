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
		App app(1280, 720, "Video Player");
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << "Application Error: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}