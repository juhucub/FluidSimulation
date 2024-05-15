#pragma once
#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "demoShader.h"

class Circle {
public:
    Circle(int numParticles, float radius, glm::vec3 color);
    ~Circle();

    void update(float deltaTime);
    void initRenderData();
    void draw(const Shader &shader);

    void setGravity(float gravity);
    void setCollisionDampening(float d);
    void setRadius(float radius);
    void setColor(const glm::vec3& color);
    void setNumParticles(int numParticles);

private:
    void applyGravity(float deltaTime);
    void keepInBounds();
    void handleCollisions();

 std::vector<glm::vec2> positions;
    std::vector<glm::vec2> velocities;
    float radius;
    glm::vec3 color;
    float dampingFactor;
    float gravity;

    unsigned int VAO, VBO;
};

