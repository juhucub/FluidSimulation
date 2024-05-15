
#include "Circle.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "demoShader.h"

const float boundaryTop = 1.0f;
const float boundaryBottom = -1.0f;
const float boundaryLeft = -1.0f;
const float boundaryRight = 1.0f;

Circle::Circle(float x, float y, float radius, glm::vec3 color)
    : position(x, y), radius(radius), color(color), velocity(0.0f, 0.0f), dampingFactor(0.9f), gravity(0.98f) {
        initRenderData();
    }

Circle::~Circle() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void Circle::keepInBounds() {
    if (position.y - radius < boundaryBottom) {
        position.y = boundaryBottom + radius;
        velocity.y *= -dampingFactor; // Invert velocity to bounce
    }
    if (position.y + radius > boundaryTop) {
        position.y = boundaryTop - radius;
        velocity.y *= -dampingFactor;
    }
    if (position.x - radius < boundaryLeft) {
        position.x = boundaryLeft + radius;
        velocity.x *= -dampingFactor;
    }
    if (position.x + radius > boundaryRight) {
        position.x = boundaryRight - radius;
        velocity.x *= -dampingFactor;
    }
}

void Circle::update(float deltaTime) {
    applyGravity(deltaTime);
    position += velocity * deltaTime;
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
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
    glm::mat4 projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    shader.bind();
    glUniformMatrix4fv(shader.getUniform("model"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(shader.getUniform("projection"), 1, GL_FALSE, &projection[0][0]);
    glUniform3fv(shader.getUniform("circleColor"), 1, &color[0]);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 102);
    glBindVertexArray(0);
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
    velocity.y -= gravity * deltaTime;
}