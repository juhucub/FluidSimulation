#version 410 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec4 aMetrics;

uniform mat4 view;
uniform mat4 projection;
uniform float particleWorldRadius;
uniform float viewportHeight;

out vec4 vMetrics;

void main() {
    vec4 viewPosition = view * vec4(aPosition, 1.0);
    float distanceToCamera = max(-viewPosition.z, 0.15);
    float projectedRadius = projection[1][1] * particleWorldRadius * viewportHeight / distanceToCamera;

    vMetrics = aMetrics;
    gl_PointSize = clamp(projectedRadius * 2.15, 3.0, 104.0);
    gl_Position = projection * viewPosition;
}
