#pragma once
#include<iostream>
#include <string>
#include <fstream>
#include <sstream>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

class ShaderProgram
{
public:
	GLuint programID;

	ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath)
	{
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;

        // 1. Open files
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);

        if (!vShaderFile.is_open()) {
            std::cerr << "FATAL ERROR: Could not open Vertex Shader at: " << vertexPath << std::endl;
            return;
        }
        if (!fShaderFile.is_open()) {
            std::cerr << "FATAL ERROR: Could not open Fragment Shader at: " << fragmentPath << std::endl;
            return;
        }

        // 2. Read contents
        std::stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vShaderFile.close();
        fShaderFile.close();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();

        // 3. Debug: Print the first 20 characters to see if they loaded
        //std::cout << "Vertex Shader Load Success. Starts with: " << vertexCode.substr(0, 15) << "..." << std::endl;

        // 4. Compile and Link
        GLuint v = CompileShader(GL_VERTEX_SHADER, vertexCode.c_str());
        GLuint f = CompileShader(GL_FRAGMENT_SHADER, fragmentCode.c_str());

        programID = glCreateProgram();
        glAttachShader(programID, v);
        glAttachShader(programID, f);
        glLinkProgram(programID);

        // IMPORTANT: Check linking here
        CheckCompileErrors(programID, "PROGRAM");

        glDeleteShader(v);
        glDeleteShader(f);
	}

	~ShaderProgram() 
    {
        glDeleteProgram(programID);
    }

    void Use()
    {
        glUseProgram(programID);
    }

    void SetUniformIntValue(const std::string& uniformName, GLint value)
    {
        Use();
        glUniform1i(glGetUniformLocation(programID, uniformName.c_str()), value);
        glUniform1i(glGetUniformLocation(programID, uniformName.c_str()), value);
    }

    void SetTextureUnits()
    {
        Use();
        SetUniformIntValue("yTexture", 0);
        SetUniformIntValue("uvTexture", 1);
    }

private:
	GLuint CompileShader(GLenum shaderType, const char* src)
	{
		GLuint shader = glCreateShader(shaderType);
		glShaderSource(shader, 1, &src, NULL);
		glCompileShader(shader);
        CheckCompileErrors(shader, (shaderType == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"));

        return shader;
	}

    void CheckCompileErrors(GLuint shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};