#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

class Shader {
public:
    unsigned int id;

    Shader();
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();

    bool loadShaderProgramFromFile(const char *vertexShaderPath, const char *fragmentShaderPath);
    bool loadShaderProgramFromFile(const char *vertexShaderPath, const char *geometryShaderPath, const char *fragmentShaderPath);
    bool loadShaderProgramFromData(const char *vertexShaderData, const char *fragmentShaderData);
    bool loadShaderProgramFromData(const char *vertexShaderData, const char *geometryShaderData, const char *fragmentShaderData);
    void use() const;
    void clear();
    GLint getUniform(const char *name) const;
    GLuint getProgramID() const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;

private:
    bool compileShader(GLuint shader, const char *source, const std::string &shaderType);
    bool linkProgram(GLuint vertexShader, GLuint fragmentShader, GLuint geometryShader = 0);
    std::string readShaderFile(const char *filePath);
};
