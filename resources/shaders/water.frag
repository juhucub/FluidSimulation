#version 410 core

in vec4 vMetrics;

out vec4 FragColor;

uniform vec3 lightDirectionView;
uniform vec3 shallowColor;
uniform vec3 deepColor;
uniform vec3 foamColor;
uniform vec3 skyColor;
uniform float waterAlpha;
uniform float ambientStrength;
uniform float specularStrength;
uniform float fresnelStrength;
uniform int debugMode;

vec3 heatMap(float value) {
    float t = clamp(value, 0.0, 1.0);
    vec3 cold = vec3(0.07, 0.24, 0.57);
    vec3 mid = vec3(0.12, 0.73, 0.88);
    vec3 warm = vec3(1.00, 0.88, 0.26);
    vec3 hot = vec3(0.94, 0.25, 0.20);
    if (t < 0.33) {
        return mix(cold, mid, t / 0.33);
    }
    if (t < 0.66) {
        return mix(mid, warm, (t - 0.33) / 0.33);
    }
    return mix(warm, hot, (t - 0.66) / 0.34);
}

void main() {
    vec2 particleUv = gl_PointCoord * 2.0 - 1.0;
    float radiusSquared = dot(particleUv, particleUv);
    if (radiusSquared > 1.0) {
        discard;
    }

    float sphereZ = sqrt(max(1.0 - radiusSquared, 0.0));
    vec3 normal = normalize(vec3(particleUv.x, particleUv.y, sphereZ));
    vec3 viewDirection = vec3(0.0, 0.0, 1.0);
    vec3 lightDirection = normalize(lightDirectionView);

    float density = clamp(vMetrics.x, 0.0, 2.0);
    float speed = clamp(vMetrics.y, 0.0, 1.0);
    float pressure = clamp(vMetrics.z, 0.0, 1.0);
    float interaction = clamp(vMetrics.w, 0.0, 1.0);

    if (debugMode == 1) {
        FragColor = vec4(heatMap(clamp(density * 0.55, 0.0, 1.0)), 1.0);
        return;
    }
    if (debugMode == 2) {
        FragColor = vec4(heatMap(speed), 1.0);
        return;
    }
    if (debugMode == 3) {
        FragColor = vec4(heatMap(pressure), 1.0);
        return;
    }
    if (debugMode == 4) {
        FragColor = vec4(mix(deepColor, foamColor, interaction), 1.0);
        return;
    }

    vec3 halfVector = normalize(lightDirection + viewDirection);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float fresnel = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.5);
    float specular = pow(max(dot(normal, halfVector), 0.0), 28.0 + pressure * 12.0) * specularStrength;
    float volume = smoothstep(0.0, 1.0, sphereZ);
    float edgeFade = smoothstep(0.0, 0.18, sphereZ);

    vec3 baseColor = mix(deepColor, shallowColor, clamp(0.22 + density * 0.38 + speed * 0.10, 0.0, 1.0));
    vec3 litColor = baseColor * (ambientStrength + diffuse * (1.0 - ambientStrength));
    litColor += skyColor * fresnel * fresnelStrength;
    litColor += vec3(specular);

    float foamBlend = clamp((density - 1.0) * 0.22 + speed * 0.12 + interaction * 0.36, 0.0, 0.68);
    vec3 finalColor = mix(litColor, foamColor, foamBlend);
    float alpha = clamp((0.34 + volume * 0.66) * edgeFade * waterAlpha, 0.0, 0.95);

    FragColor = vec4(finalColor, alpha);
}
