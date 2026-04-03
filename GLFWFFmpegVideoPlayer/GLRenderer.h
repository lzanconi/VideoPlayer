#pragma once
#include <iostream>
#include <vector>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

class GLRenderer
{
public:
	GLRenderer(int width, int height, const char* title)
	{
		if (!glfwInit())
			exit(-1);

		window = glfwCreateWindow(width, height, title, NULL, NULL);
		if (!window) 
		{ 
			glfwTerminate(); 
			exit(-1); 
		}

		glfwMakeContextCurrent(window);
		gladLoaderLoadGL();
		glfwSwapInterval(1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		SetupGeometry();
		SetupTextures();
	}

	~GLRenderer()
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteTextures(1, &yTex);
		glDeleteTextures(1, &uvTex);
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void UpdateVideoTextures(int w, int h, int lsY, uint8_t* dY, int lsUV, uint8_t* dUV)
	{
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, yTex);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, lsY);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, dY);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, uvTex);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, lsUV / 2);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, w / 2, h / 2, 0, GL_RG, GL_UNSIGNED_BYTE, dUV);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}

	void ToggleFullscreen()
	{
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		if (!isFullscreen) {
			// Save current windowed position and size
			glfwGetWindowPos(window, &winX, &winY);
			glfwGetWindowSize(window, &winW, &winH);
			// Switch to Fullscreen
			glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		}
		else {
			// Switch back to Windowed using saved values
			glfwSetWindowMonitor(window, NULL, winX, winY, winW, winH, 0);
		}
		isFullscreen = !isFullscreen;
		glfwSwapInterval(1); // Reset V-Sync after mode switch
	}

	void Render(unsigned int shaderProgramID)
	{
		// Handling aspect ratio/viewport
		int dw, dh;
		glfwGetFramebufferSize(window, &dw, &dh);
		// Note: In a real app, you'd pass the video aspect ratio here to calculate the viewport
		glViewport(0, 0, dw, dh);

		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(shaderProgramID);
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	bool ShouldClose() { return glfwWindowShouldClose(window); }
	void SwapBuffers() { glfwSwapBuffers(window); }
	void PollEvents() { glfwPollEvents(); }
	void SetKeyCallback(GLFWkeyfun cb) { glfwSetKeyCallback(window, cb); }

private:
	GLFWwindow* window;
	unsigned int VAO, VBO, EBO;
	unsigned int yTex, uvTex;
	bool isFullscreen = false;
	int winX = 100, winY = 100, winW = 1280, winH = 720;

	void SetupGeometry()
	{
		float vertices[] = {
		 1.f,  1.f, 0.f,  1.f, 1.f,
		 1.f, -1.f, 0.f,  1.f, 0.f,
		-1.f, -1.f, 0.f,  0.f, 0.f,
		-1.f,  1.f, 0.f,  0.f, 1.f
		};
		unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
	}

	void SetupTextures()
	{
		glGenTextures(1, &yTex);
		glGenTextures(1, &uvTex);
		auto initTex = [](GLuint t) {
			glBindTexture(GL_TEXTURE_2D, t);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			};
		initTex(yTex); initTex(uvTex);
	}
};
