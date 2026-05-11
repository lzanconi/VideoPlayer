#pragma once
#include <string>
#include <glad/gl.h>

class ShaderProgram
{
public:
    GLuint programID;

    // Constructor: Loads, compiles, and links the shader program
    ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);

    // Destructor: Cleans up the shader program from the GPU
    ~ShaderProgram();

    // Activates the shader program for rendering
    void Use();

    // Utility functions to set uniforms
    void SetUniformIntValue(const std::string& uniformName, GLint value);
    void SetUniformFloat(const std::string& uniformName, float value);

    // Configures the default texture units for Y and UV textures
    void SetTextureUnits();

private:
    // Helper to compile individual shader stages
    GLuint CompileShader(GLenum shaderType, const char* src);

    // Checks for compilation or linking errors
    void CheckCompileErrors(GLuint shader, std::string type);
};