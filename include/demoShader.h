#pragma once
#include <glad/glad.h>
#include <string>

class Shader {
public:
    Shader();
    ~Shader();

    bool loadShaderProgramFromData(const char *vertexShaderData, const char *fragmentShaderData);
    bool loadShaderProgramFromData(const char *vertexShaderData, const char *geometryShaderData, const char *fragmentShaderData);
    bool loadShaderProgramFromFile(const char *vertexShaderPath, const char *fragmentShaderPath);
    bool loadShaderProgramFromFile(const char *vertexShaderPath, const char *geometryShaderPath, const char *fragmentShaderPath);

    void bind() const;
    void clear();

    GLint getUniform(const char *name) const;
    GLuint getProgramID() const;

private:
    GLuint id;

    bool compileShader(GLuint shader, const char *source, const std::string &shaderType);
    bool linkProgram(GLuint vertexShader, GLuint fragmentShader, GLuint geometryShader = 0);
    std::string readShaderFile(const char *filePath);
};

GLint getUniform(GLuint shaderId, const char *name);