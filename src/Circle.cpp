#include "Circle.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

const float gravity = 9.8f;
const float boundaryTop = 1.0f;
const float boundaryBottom = -1.0f;
const float boundaryLeft = -1.0f;
const float boundaryRight = 1.0f;

Circle::Circle(float x, float y, float radius, glm::vec3 color)
    : position(x, y), radius(radius), color(color), velocity(0.0f, 0.0f) {}

void Circle::applyGravity(float deltaTime) {
    velocity.y -= gravity * deltaTime;
}

void Circle::keepInBounds() {
    if (position.y - radius < boundaryBottom) {
        position.y = boundaryBottom + radius;
        velocity.y *= -1; // Invert velocity to bounce
    }
    if (position.y + radius > boundaryTop) {
        position.y = boundaryTop - radius;
        velocity.y *= -1;
    }
    if (position.x - radius < boundaryLeft) {
        position.x = boundaryLeft + radius;
        velocity.x *= -1;
    }
    if (position.x + radius > boundaryRight) {
        position.x = boundaryRight - radius;
        velocity.x *= -1;
    }
}

void Circle::update(float deltaTime) {
    applyGravity(deltaTime);
    position += velocity * deltaTime;
    keepInBounds();
}

void Circle::draw() {
    const int numSegments = 100;
    float theta = 2 * 3.1415926f / float(numSegments);
    float tangetialFactor = tanf(theta);
    float radialFactor = cosf(theta);

    float x = radius;
    float y = 0;

    glBegin(GL_LINE_LOOP);
    glColor3f(color.r, color.g, color.b);

    for(int i = 0; i < numSegments; i++) {
        glVertex2f(x + position.x, y + position.y);

        float tx = -y;
        float ty = x;

        x += tx * tangetialFactor;
        y += ty * tangetialFactor;

        x *= radialFactor;
        y *= radialFactor;
    }

    glEnd();
}
