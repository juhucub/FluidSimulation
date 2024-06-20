#pragma once
#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "demoShader.h"

const int THREAD_COUNT = 9;
const float GRAVITY = -9.8f;

struct CircleObject {
    glm::vec3 current;
    glm::vec3 previous;
    glm::vec3 acceleration;
    float radius;

    CircleObject(float x, float y, float z, float r)
        : current(x, y, z), previous(x, y, z), radius(r) {
            acceleration = glm::vec3(0.0f);
        }
}; 

class Circle {
public:
    std::vector<CircleObject> objects;
    bool running;

    Circle(int numParticles, float radius);
    void addObject(const CircleObject& obj);
    void applyForces();
    void applyCollisions();
    void applyConstraints(const glm::vec3& containerPosition, float containerRadius);
    void updatePositions(float dt);
    void runSimulation(float dt, const glm::vec3& containerPosition, float containerRadius);

private:
    void handleCollision(CircleObject& a, CircleObject& b);
};
    
