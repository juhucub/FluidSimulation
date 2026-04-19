#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

class OrbitCamera {
public:
    OrbitCamera();

    void setViewport(int width, int height);
    void update(GLFWwindow* window, float deltaTime, bool blockMouseInput);
    void processScroll(double yoffset);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getPosition() const;
    glm::vec3 getTarget() const;
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;
    Ray screenPointToRay(double mouseX, double mouseY) const;

private:
    glm::vec3 positionFromOrbit() const;

    glm::vec3 target_;
    float yaw_;
    float pitch_;
    float distance_;
    float fieldOfViewDegrees_;
    float aspectRatio_;
    int viewportWidth_;
    int viewportHeight_;
    float orbitSensitivity_;
    float panSensitivity_;
    double lastMouseX_;
    double lastMouseY_;
    bool mouseInitialized_;
};

void registerCameraCallbacks(GLFWwindow* window, OrbitCamera* camera);
