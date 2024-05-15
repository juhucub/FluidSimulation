#pragma once
#include <glm/glm.hpp>
#include "demoShader.h"

class Circle {
public:
    Circle(float x, float y, float radius, glm::vec3 color);
    ~Circle();

    void update(float deltaTime);
    void initRenderData();
    void draw(const Shader &shader);

    glm::vec2 position;
    float radius;
    glm::vec3 color;
    glm::vec2 velocity;
    float dampingFactor;

private:
    void applyGravity(float deltaTime);
    void keepInBounds();


    unsigned int VAO, VBO;
};

