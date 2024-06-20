
#include "Circle.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <random>
#include <iostream>
#include <thread>
#include <chrono>
#include "demoShader.h"



Circle::Circle(int numParticles, float radius) {
    running = true;
    for (int i = 0; i < numParticles; ++i) {
        float angle = (i / (float)numParticles) * 3.14159f * 2.0f;
        float x = std::cos(angle) * radius;
        float y = std::sin(angle) * radius;
        addObject(CircleObject(x, y, 0.0f, radius));
    }
}
    
void Circle::addObject(const CircleObject& obj) {
    std::cerr << "Object placed" << std::endl;
    objects.push_back(obj);
}

void Circle::applyForces() {
    for (auto& obj : objects) {
        obj.acceleration.y += GRAVITY;
    }
}

void Circle::handleCollision(CircleObject& a, CircleObject& b) {
    glm::vec3 axis = a.current - b.current;
    float dist = glm::length(axis);
    if (dist < a.radius + b.radius) {
        glm::vec3 norm = glm::normalize(axis);
        float delta = a.radius + b.radius - dist;
        glm::vec3 correction = norm * (0.5f * delta);
        a.current += correction;
        b.current -= correction;
    }
}


void Circle::applyCollisions() {
    for (size_t a = 0; a < objects.size(); ++a) {
        for (size_t b = a + 1; b < objects.size(); ++b) {
            handleCollision(objects[a], objects[b]);
        }
    }
}


void Circle::applyConstraints(const glm::vec3& containerPosition, float containerRadius) {
    for (auto& obj : objects) {
        glm::vec3 toCenter = obj.current - containerPosition;
        float dist = glm::length(toCenter);
        if (dist > containerRadius - obj.radius) {
            glm::vec3 norm = glm::normalize(toCenter);
            obj.current = containerPosition + norm * (containerRadius - obj.radius);
        }
    }
}

void Circle::updatePositions(float dt) {
    for (auto& obj : objects) {
        glm::vec3 displacement = obj.current - obj.previous;
        obj.previous = obj.current;
        obj.acceleration *= dt * dt;
        obj.current += displacement + obj.acceleration;
        obj.acceleration = glm::vec3(0.0f);
    }
}

void Circle::runSimulation(float dt, const glm::vec3& containerPosition, float containerRadius) {
    while (running) {
        applyForces();
        applyCollisions();
        applyConstraints(containerPosition, containerRadius);
        updatePositions(dt);
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // simulate 60 FPS
    }
}

