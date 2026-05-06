# VideoPlayer

A **C++ FFMPEG + GLFW + GLAD Zero-Copy** video player for Intel integrated GPU and NVidia GPU on Windows

## Dependencies

 - Visual Studio 2022
 - GLFW (OpenGL Context)
 - GLAD (OpenGL Extensions)
 - FFmpeg (hardware video decoding)

## Visual Studio Setup

**1.GLFW**<br>
Download **GLFW Windows pre-compiled binaries** [here](https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip)<br>
Extract the content to a folder where we'll store all the required libraries (e.g. C:\Libraries\glfw)

**2.GLAD**<br>
Go to GLAD2 generator [website](https://gen.glad.sh/)<br>
Configure the generator as follows:<br>
Under **gl** select **Version 4.6** and on the right, where it says **Compatibility** select **Core**<br>
Under **wgl** select **Version 1.0**<br> 
Under **Extensions** search for **WGL_NV_DX_interop** then click on `WGL_NV_DX_interop`<br>
Under **Options** check **loader** 
Click on **Generate** then download **glad.zip** file<br>
Extract the content inside C:\Libraries\glad

**3.FFmpeg**<br>
Download **ffmpeg build for Windows** [here](https://www.gyan.dev/ffmpeg/builds/)<br>
Extract the content to a folder where we'll store all the required libraries (e.g. C:\Libraries\ffmpeg)

**4.Setup Visual Studio Include directories**
Open **project settings**, then go to **C/C++** -> **General**<br> 
For **Additional Include Directories** add the following directories:<br><br>
C:\Libraries\glfw\include<br>
C:\Libraries\ffmpeg\include<br>
C:\Libraries\glad\include<br>
<br>

Open **project settings**, then go to **Linker** -> **General**<br> 
For **Additional Library Directories** add the following directories:<br><br>
C:\Libraries\glfw\lib-vc2022<br>
C:\Libraries\ffmpeg\lib<br><br>


## Build the program

## Add Video folder
Inside **x64\Debug** build folder, create a **Videos** folder and put your videos inside of it<br>

## RUN
Go to the **PythonCode** folder and run the positions manager emulator:<br>
**python PositionsManagerEmu.py**<br><br>

Run the application from Visual Studio (or from the executable).
Press **enter** to start the background video and it should send the positions to the 
position manager emulator.




