#include "demoShader.h"
#include <iostream>
#include <fstream>
#include <sstream>

// Utility function to create a shader from source code
bool Shader::compileShader(GLuint shader, const char *source, const std::string &shaderType) {
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        std::string infoLog(maxLength, ' ');
        glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

        std::cerr << "Error compiling " << shaderType << " shader:\n" << infoLog << std::endl;
        return false;
    }
    return true;
}

// Utility function to link shader program
bool Shader::linkProgram(GLuint vertexShader, GLuint fragmentShader, GLuint geometryShader) {
    id = glCreateProgram();
    glAttachShader(id, vertexShader);
    glAttachShader(id, fragmentShader);
    if (geometryShader) {
        glAttachShader(id, geometryShader);
    }
    glLinkProgram(id);

    GLint success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        GLint maxLength = 0;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &maxLength);

        std::string infoLog(maxLength, ' ');
        glGetProgramInfoLog(id, maxLength, &maxLength, &infoLog[0]);

        std::cerr << "Error linking shader program:\n" << infoLog << std::endl;
        glDeleteProgram(id);
        return false;
    }
    return true;
}

// Read shader file
std::string Shader::readShaderFile(const char *filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filePath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Shader class methods
Shader::Shader() : id(0) {}

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    loadShaderProgramFromFile(vertexPath, fragmentPath);
}

Shader::~Shader() {
    clear();
}

bool Shader::loadShaderProgramFromData(const char *vertexShaderData, const char *fragmentShaderData) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compileShader(vertexShader, vertexShaderData, "vertex") || !compileShader(fragmentShader, fragmentShaderData, "fragment")) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    if (!linkProgram(vertexShader, fragmentShader)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

bool Shader::loadShaderProgramFromData(const char *vertexShaderData, const char *geometryShaderData, const char *fragmentShaderData) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compileShader(vertexShader, vertexShaderData, "vertex") || !compileShader(geometryShader, geometryShaderData, "geometry") || !compileShader(fragmentShader, fragmentShaderData, "fragment")) {
        glDeleteShader(vertexShader);
        glDeleteShader(geometryShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    if (!linkProgram(vertexShader, fragmentShader, geometryShader)) {
        glDeleteShader(vertexShader);
        glDeleteShader(geometryShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(geometryShader);
    glDeleteShader(fragmentShader);

    return true;
}

bool Shader::loadShaderProgramFromFile(const char *vertexShaderPath, const char *fragmentShaderPath) {
    std::string vertexShaderData = readShaderFile(vertexShaderPath);
    std::string fragmentShaderData = readShaderFile(fragmentShaderPath);

    return loadShaderProgramFromData(vertexShaderData.c_str(), fragmentShaderData.c_str());
}

bool Shader::loadShaderProgramFromFile(const char *vertexShaderPath, const char *geometryShaderPath, const char *fragmentShaderPath) {
    std::string vertexShaderData = readShaderFile(vertexShaderPath);
    std::string geometryShaderData = readShaderFile(geometryShaderPath);
    std::string fragmentShaderData = readShaderFile(fragmentShaderPath);

    return loadShaderProgramFromData(vertexShaderData.c_str(), geometryShaderData.c_str(), fragmentShaderData.c_str());
}

void Shader::use() const {
    glUseProgram(id);
}

void Shader::clear() {
    if (id != 0) {
        glDeleteProgram(id);
        id = 0;
    }
}

GLint Shader::getUniform(const char *name) const {
    return glGetUniformLocation(id, name);
}

GLuint Shader::getProgramID() const {
    return id;
}

void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const {
    glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

GLint getUniform(GLuint shaderId, const char *name) {
    GLint uniform = glGetUniformLocation(shaderId, name);
    if (uniform == -1) {
        std::cerr << "Error retrieving uniform location for " << name << std::endl;
    }
    return uniform;
}
