#pragma once

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "sim/WaterSimulation.h"
#include "util/gl/demoShader.h"

struct WaterPalette {
    glm::vec3 shallowColor = glm::vec3(0.36f, 0.74f, 0.96f);
    glm::vec3 deepColor = glm::vec3(0.05f, 0.25f, 0.47f);
    glm::vec3 foamColor = glm::vec3(0.93f, 0.98f, 1.0f);
    glm::vec3 skyColor = glm::vec3(0.73f, 0.86f, 0.98f);
    glm::vec3 basinColor = glm::vec3(0.68f, 0.74f, 0.79f);
    glm::vec3 accentColor = glm::vec3(0.20f, 0.46f, 0.70f);
};

struct WaterRenderSettings {
    glm::vec3 lightDirection = glm::normalize(glm::vec3(-0.35f, 1.0f, 0.22f));
    float waterAlpha = 0.88f;
    float particleScale = 1.95f;
    float ambientStrength = 0.38f;
    float specularStrength = 0.15f;
    float fresnelStrength = 0.12f;
    float basinFloorY = -1.15f;
    float basinLipY = 0.2f;
};

class WaterRenderer {
public:
    WaterRenderer();
    ~WaterRenderer();

    bool initialize(const std::string& resourceDirectory, const WaterRenderSnapshot& snapshot);
    void updateSurface(const WaterRenderSnapshot& snapshot);
    void setInteractionIndicator(bool visible, const glm::vec3& worldPoint, float radius, float intensity);
    void render(
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& cameraPosition,
        const WaterPalette& palette,
        const WaterRenderSettings& renderSettings,
        int debugMode,
        float timeSeconds,
        int viewportHeight
    );

private:
    struct ParticleVertex {
        glm::vec3 position;
        glm::vec4 metrics;
    };

    struct SolidVertex {
        glm::vec3 position;
        glm::vec3 normal;
    };

    void ensureParticleCapacity(std::size_t particleCount);
    void buildBasinMesh(float halfExtent, float floorY, float lipY);
    void updateInteractionRing();
    void clear();

    Shader waterShader_;
    Shader basinShader_;
    Shader lineShader_;

    GLuint particleVao_;
    GLuint particleVbo_;
    GLuint basinVao_;
    GLuint basinVbo_;
    GLuint lineVao_;
    GLuint lineVbo_;
    GLuint lineEbo_;
    GLuint interactionVao_;
    GLuint interactionVbo_;

    std::vector<SolidVertex> basinVertices_;
    std::vector<glm::vec3> lineVertices_;
    std::vector<unsigned int> lineIndices_;
    std::vector<glm::vec3> interactionVertices_;

    std::size_t particleCapacity_;
    std::size_t particleCount_;
    int lineIndexCount_;
    bool interactionVisible_;
    float particleWorldRadius_;
    glm::vec3 interactionCenter_;
    float interactionRadius_;
    float interactionIntensity_;
};
