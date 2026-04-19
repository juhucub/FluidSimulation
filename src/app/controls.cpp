#include "app/controls.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {
OrbitCamera* gCamera = nullptr;

void scrollCallback(GLFWwindow*, double, double yoffset) {
    if (gCamera != nullptr) {
        gCamera->processScroll(yoffset);
    }
}
} // namespace

OrbitCamera::OrbitCamera()
    : target_(0.0f, 0.0f, 0.0f),
      yaw_(glm::radians(35.0f)),
      pitch_(glm::radians(-28.0f)),
      distance_(9.5f),
      fieldOfViewDegrees_(45.0f),
      aspectRatio_(16.0f / 9.0f),
      viewportWidth_(1280),
      viewportHeight_(720),
      orbitSensitivity_(0.0055f),
      panSensitivity_(0.0018f),
      lastMouseX_(0.0),
      lastMouseY_(0.0),
      mouseInitialized_(false) {}

void OrbitCamera::setViewport(int width, int height) {
    viewportWidth_ = std::max(width, 1);
    viewportHeight_ = std::max(height, 1);
    if (height <= 0) {
        height = 1;
    }
    aspectRatio_ = static_cast<float>(width) / static_cast<float>(height);
}

void OrbitCamera::update(GLFWwindow* window, float deltaTime, bool blockMouseInput) {
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if (!mouseInitialized_) {
        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;
        mouseInitialized_ = true;
    }

    const float deltaX = static_cast<float>(mouseX - lastMouseX_);
    const float deltaY = static_cast<float>(mouseY - lastMouseY_);
    lastMouseX_ = mouseX;
    lastMouseY_ = mouseY;

    if (!blockMouseInput && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        yaw_ -= deltaX * orbitSensitivity_;
        pitch_ -= deltaY * orbitSensitivity_;
        const float maxPitch = glm::radians(80.0f);
        pitch_ = std::clamp(pitch_, -maxPitch, maxPitch);
    }

    if (!blockMouseInput && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        const float panScale = distance_ * panSensitivity_ * std::max(0.3f, deltaTime * 60.0f);
        target_ -= getRight() * deltaX * panScale;
        target_ += getUp() * deltaY * panScale;
    }
}

void OrbitCamera::processScroll(double yoffset) {
    distance_ *= (1.0f - static_cast<float>(yoffset) * 0.1f);
    distance_ = std::clamp(distance_, 2.5f, 25.0f);
}

glm::mat4 OrbitCamera::getViewMatrix() const {
    return glm::lookAt(getPosition(), target_, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 OrbitCamera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(fieldOfViewDegrees_), aspectRatio_, 0.05f, 100.0f);
}

glm::vec3 OrbitCamera::getPosition() const {
    return positionFromOrbit();
}

glm::vec3 OrbitCamera::getTarget() const {
    return target_;
}

glm::vec3 OrbitCamera::getForward() const {
    return glm::normalize(target_ - getPosition());
}

glm::vec3 OrbitCamera::getRight() const {
    return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 OrbitCamera::getUp() const {
    return glm::normalize(glm::cross(getRight(), getForward()));
}

Ray OrbitCamera::screenPointToRay(double mouseX, double mouseY) const {
    const glm::mat4 inverseViewProjection = glm::inverse(getProjectionMatrix() * getViewMatrix());
    const float x = (2.0f * static_cast<float>(mouseX) / static_cast<float>(viewportWidth_)) - 1.0f;
    const float y = 1.0f - (2.0f * static_cast<float>(mouseY) / static_cast<float>(viewportHeight_));

    glm::vec4 nearPoint = inverseViewProjection * glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(x, y, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    Ray ray;
    ray.origin = glm::vec3(nearPoint);
    ray.direction = glm::normalize(glm::vec3(farPoint - nearPoint));
    return ray;
}

glm::vec3 OrbitCamera::positionFromOrbit() const {
    const float cosPitch = std::cos(pitch_);
    const glm::vec3 orbitOffset(
        std::sin(yaw_) * cosPitch,
        std::sin(pitch_),
        std::cos(yaw_) * cosPitch
    );
    return target_ - orbitOffset * distance_;
}

void registerCameraCallbacks(GLFWwindow* window, OrbitCamera* camera) {
    gCamera = camera;
    glfwSetScrollCallback(window, scrollCallback);
}
