#version 410 core

in vec3 vWorldPosition;
in vec3 vNormal;

out vec4 FragColor;

uniform vec3 cameraPosition;
uniform vec3 lightDirection;
uniform vec3 basinColor;
uniform vec3 accentColor;
uniform float timeSeconds;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(cameraPosition - vWorldPosition);
    vec3 L = normalize(lightDirection);
    vec3 H = normalize(V + L);

    float diffuse = max(dot(N, L), 0.0);
    float specular = pow(max(dot(N, H), 0.0), 24.0) * 0.08;
    vec2 gridUv = vWorldPosition.xz * 1.4;
    vec2 grid = abs(fract(gridUv - 0.5) - 0.5) / fwidth(gridUv);
    float line = 1.0 - min(min(grid.x, grid.y), 1.0);
    float pulse = 0.5 + 0.5 * sin(timeSeconds * 0.2 + vWorldPosition.x * 0.4 + vWorldPosition.z * 0.3);

    vec3 color = basinColor * (0.55 + 0.45 * diffuse);
    color = mix(color, accentColor, line * 0.18 + pulse * 0.03);
    color += vec3(specular);

    FragColor = vec4(color, 1.0);
}
