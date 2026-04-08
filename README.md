# VideoPlayer

A **C++ FFMPEG + GLFW + GLAD Zero-Copy** video player for Intel integrated GPU and NVidia GPU on Windows


## Dependencies

 - Visual Studio 2022
 - GLFW (OpenGL Context)
 - GLAD (OpenGL Extensions)
 - FFmpeg (hardware video decoding)

## Visual Studio Setup

**1.GLFW**
Download **GLFW Windows pre-compiled binaries** [here](https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip)
Extract the content to a folder where we'll store all the required libraries (e.g. C:\Libraries\glfw)

**2.GLAD**
Go to GLAD2 generator [website](https://gen.glad.sh/)
Configure the generator as follows:
Under **gl** select **Version 4.6** and on the right, where it says **Compatibility** select **Core**
Under **wgl** select **Version 1.0** 
Under **Extensions** search for **WGL_NV_DX_interop** then click on `WGL_NV_DX_interop`
Under **Options** check **loader** 
Click on **Generate** then download **glad.zip** file
Extract the content inside C:\Libraries\glad

**3.FFmpeg**
