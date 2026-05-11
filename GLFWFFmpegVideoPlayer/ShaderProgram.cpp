#include "ShaderProgram.h"
#include <iostream>
#include <fstream>
#include <sstream>

ShaderProgram::ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath)
{
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    // 1. Open shader files
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

    // 2. Read contents into streams
    std::stringstream vShaderStream, fShaderStream;
    vShaderStream << vShaderFile.rdbuf();
    fShaderStream << fShaderFile.rdbuf();

    vShaderFile.close();
    fShaderFile.close();

    vertexCode = vShaderStream.str();
    fragmentCode = fShaderStream.str();

    // 3. Compile and Link
    GLuint v = CompileShader(GL_VERTEX_SHADER, vertexCode.c_str());
    GLuint f = CompileShader(GL_FRAGMENT_SHADER, fragmentCode.c_str());

    programID = glCreateProgram();
    glAttachShader(programID, v);
    glAttachShader(programID, f);
    glLinkProgram(programID);

    // Check for linking errors
    CheckCompileErrors(programID, "PROGRAM");

    // Clean up individual shaders as they are now linked into the program
    glDeleteShader(v);
    glDeleteShader(f);
}

ShaderProgram::~ShaderProgram()
{
    glDeleteProgram(programID);
}

void ShaderProgram::Use()
{
    glUseProgram(programID);
}

void ShaderProgram::SetUniformIntValue(const std::string& uniformName, GLint value)
{
    Use();
    glUniform1i(glGetUniformLocation(programID, uniformName.c_str()), value);
}

void ShaderProgram::SetUniformFloat(const std::string& uniformName, float value)
{
    glUniform1f(glGetUniformLocation(programID, uniformName.c_str()), value);
}

void ShaderProgram::SetTextureUnits()
{
    Use();
    SetUniformIntValue("yTexture", 0);
    SetUniformIntValue("uvTexture", 1);
}

GLuint ShaderProgram::CompileShader(GLenum shaderType, const char* src)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    CheckCompileErrors(shader, (shaderType == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"));

    return shader;
}

void ShaderProgram::CheckCompileErrors(GLuint shader, std::string type)
{
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}