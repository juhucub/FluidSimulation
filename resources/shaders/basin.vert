#version 410 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vWorldPosition;
out vec3 vNormal;

void main() {
    vec4 worldPosition = model * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vNormal = normalize(mat3(model) * aNormal);
    gl_Position = projection * view * worldPosition;
}
