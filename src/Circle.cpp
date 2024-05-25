
#include "Circle.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <random>
#include <iostream>
#include "demoShader.h"

const float boundaryTop = 1.0f;
const float boundaryBottom = -1.0f;
const float boundaryLeft = -1.0f;
const float boundaryRight = 1.0f;

Circle::Circle(int numParticles, float radius, glm::vec3 color)
    : radius(radius), color(color), gravity(0.98f), dampingFactor(0.9f) {
        positions.resize(numParticles);
        velocities.resize(numParticles, glm::vec2(0.0f, 0.0f));

        //initialize particles in a grid 
        int gridSize = static_cast<int>(sqrt(numParticles));
        float spacing = radius * 2.5f;
        int index = 0;

        for(int i = 0; i < gridSize; i++) {
            for(int j = 0; j < gridSize; j++) {
                if(index < numParticles) {
                    positions[index] = glm::vec2{i * spacing - 1.0f, j * spacing - 1.0f};
                    index++;
                }
            }
        }
        
        initRenderData();
    }

Circle::~Circle() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void Circle::keepInBounds() {
    for (size_t i = 0; i < positions.size(); ++i) {
        if (positions[i].y - radius < boundaryBottom) {
            positions[i].y = boundaryBottom + radius;
            velocities[i].y *= -dampingFactor; // Invert velocity to bounce
        }
        if (positions[i].y + radius > boundaryTop) {
            positions[i].y = boundaryTop - radius;
            velocities[i].y *= -dampingFactor;
        }
        if (positions[i].x - radius < boundaryLeft) {
            positions[i].x = boundaryLeft + radius;
            velocities[i].x *= -dampingFactor;
        }
        if (positions[i].x + radius > boundaryRight) {
            positions[i].x = boundaryRight - radius;
            velocities[i].x *= -dampingFactor;
        }
    }
}

void Circle::handleCollisions() {
    for (size_t i = 0; i < positions.size(); ++i) {
        for (size_t j = i + 1; j < positions.size(); ++j) {
            glm::vec2 diff = positions[i] - positions[j];
            float dist2 = glm::dot(diff, diff);
            float minDist = radius * 2.0f;

            if (dist2 < minDist * minDist) {
                float dist = sqrt(dist2);
                glm::vec2 correction = (minDist - dist) * (diff / dist) * 0.01f;
                positions[i] += correction;
                positions[j] -= correction;

                glm::vec2 relativeVelocity = velocities[i] - velocities[j];
                float rvDotDiff = glm::dot(relativeVelocity, diff / dist);
                glm::vec2 impulse = (rvDotDiff / dist) * diff / dist;
                velocities[i] -= impulse;
                velocities[j] += impulse;
            }
        }
    }
}

void Circle::update(float deltaTime) {
    applyGravity(deltaTime);
    for (size_t i = 0; i < positions.size(); ++i) {
        positions[i] += velocities[i] * deltaTime;
    }
   // handleCollisions();
    keepInBounds();
}

void Circle::initRenderData() {
    const int numSegments = 100;
    float theta = 2 * 3.1415926f / float(numSegments);
    float tangetialFactor = tanf(theta);
    float radialFactor = cosf(theta);

    float x = radius;
    float y = 0.0f;

    std::vector<float> vertices;

    for(int i = 0; i < numSegments; i++) {
        vertices.push_back(x);
        vertices.push_back(y);

        float tx = -y;
        float ty = x;

        x += tx * tangetialFactor;
        y += ty * tangetialFactor;

        x *= radialFactor;
        y *= radialFactor;
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Circle::draw(const Shader &shader) {
    glm::mat4 projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    shader.bind();
    GLint modelLoc = shader.getUniform("model");
    GLint projectionLoc = shader.getUniform("projection");
    GLint circleColorLoc = shader.getUniform("circleColor");
    GLint alphaLoc = shader.getUniform("alpha");

    if (modelLoc == -1 || projectionLoc == -1 || circleColorLoc == -1 || alphaLoc == -1) {
        std::cerr << "Failed to get uniform locations!" << std::endl;
        return;
    }

    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);

    glBindVertexArray(VAO);
    for (const auto &position : positions) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
        glUniform3fv(circleColorLoc, 1, &color[0]);
        glUniform1f(alphaLoc, alpha); // Use the alpha value from the class

        glDrawArrays(GL_TRIANGLE_FAN, 0, 100);
        std::cerr << "Drawing circle at position: " << position.x << ", " << position.y << std::endl;
    }
    
    glBindVertexArray(0);
}

void Circle::setNumParticles(int numParticles) {
    positions.resize(numParticles);
    velocities.resize(numParticles);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> disX(boundaryLeft + radius, boundaryRight - radius);
    std::uniform_real_distribution<> disY(boundaryBottom + radius, boundaryTop - radius);

    for (int i = 0; i < numParticles; ++i) {
        positions[i] = glm::vec2(disX(gen), disY(gen));
        velocities[i] = glm::vec2(0.0f, 0.0f);
    }
}

void Circle::setGravity(float g) {
    gravity = g;
}

void Circle::setCollisionDampening(float d) {
    dampingFactor = d;
}

void Circle::setRadius(float r) {
    radius = r;
    initRenderData(); // Reinitialize render data with new radius
}

void Circle::setColor(const glm::vec3& c) {
    color = c;
}

void Circle::applyGravity(float deltaTime) {
    for (auto &velocity : velocities) {
        velocity.y -= gravity * deltaTime;
    };
}

void Circle::setAlpha(float a) {
    alpha = a;
}