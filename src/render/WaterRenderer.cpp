#include "render/WaterRenderer.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstring>

#include <glm/gtc/constants.hpp>

namespace {
constexpr int kInteractionSegments = 64;
}

WaterRenderer::WaterRenderer()
    : particleVao_(0),
      particleVbo_(0),
      basinVao_(0),
      basinVbo_(0),
      lineVao_(0),
      lineVbo_(0),
      lineEbo_(0),
      interactionVao_(0),
      interactionVbo_(0),
      particleCapacity_(0),
      particleCount_(0),
      lineIndexCount_(0),
      interactionVisible_(false),
      particleWorldRadius_(0.07f),
      interactionCenter_(0.0f),
      interactionRadius_(0.0f),
      interactionIntensity_(0.0f) {}

WaterRenderer::~WaterRenderer() {
    clear();
}

bool WaterRenderer::initialize(const std::string& resourceDirectory, const WaterRenderSnapshot& snapshot) {
    const std::string waterVertexPath = resourceDirectory + "shaders/water.vert";
    const std::string waterFragmentPath = resourceDirectory + "shaders/water.frag";
    const std::string basinVertexPath = resourceDirectory + "shaders/basin.vert";
    const std::string basinFragmentPath = resourceDirectory + "shaders/basin.frag";
    const std::string lineVertexPath = resourceDirectory + "shaders/line_vertex.vert";
    const std::string lineFragmentPath = resourceDirectory + "shaders/line_fragment.frag";

    if (!waterShader_.loadShaderProgramFromFile(waterVertexPath.c_str(), waterFragmentPath.c_str()) ||
        !basinShader_.loadShaderProgramFromFile(basinVertexPath.c_str(), basinFragmentPath.c_str()) ||
        !lineShader_.loadShaderProgramFromFile(lineVertexPath.c_str(), lineFragmentPath.c_str())) {
        return false;
    }

    ensureParticleCapacity(snapshot.particles.size());
    buildBasinMesh(snapshot.halfDomainSize, snapshot.containerFloorY, snapshot.containerLipY);
    setInteractionIndicator(false, glm::vec3(0.0f), 0.0f, 0.0f);
    updateSurface(snapshot);
    return true;
}

void WaterRenderer::updateSurface(const WaterRenderSnapshot& snapshot) {
    ensureParticleCapacity(snapshot.particles.size());
    particleWorldRadius_ = snapshot.particleRadius;
    particleCount_ = snapshot.particles.size();

    if (particleCount_ == 0) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
    void* mappedMemory = glMapBufferRange(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(particleCount_ * sizeof(ParticleVertex)),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT
    );
    if (mappedMemory != nullptr) {
        std::memcpy(mappedMemory, snapshot.particles.data(), particleCount_ * sizeof(ParticleVertex));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            static_cast<GLsizeiptr>(particleCount_ * sizeof(ParticleVertex)),
            snapshot.particles.data()
        );
    }
}

void WaterRenderer::setInteractionIndicator(bool visible, const glm::vec3& worldPoint, float radius, float intensity) {
    interactionVisible_ = visible;
    interactionCenter_ = worldPoint;
    interactionRadius_ = radius;
    interactionIntensity_ = intensity;
    updateInteractionRing();
}

void WaterRenderer::render(
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraPosition,
    const WaterPalette& palette,
    const WaterRenderSettings& renderSettings,
    int debugMode,
    float timeSeconds,
    int viewportHeight
) {
    basinShader_.use();
    basinShader_.setMat4("model", glm::mat4(1.0f));
    basinShader_.setMat4("view", view);
    basinShader_.setMat4("projection", projection);
    basinShader_.setVec3("cameraPosition", cameraPosition);
    basinShader_.setVec3("lightDirection", renderSettings.lightDirection);
    basinShader_.setVec3("basinColor", palette.basinColor);
    basinShader_.setVec3("accentColor", palette.accentColor);
    basinShader_.setFloat("timeSeconds", timeSeconds);
    glBindVertexArray(basinVao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(basinVertices_.size()));

    const glm::vec3 lightDirectionView = glm::normalize(glm::mat3(view) * renderSettings.lightDirection);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    waterShader_.use();
    waterShader_.setMat4("view", view);
    waterShader_.setMat4("projection", projection);
    waterShader_.setVec3("lightDirectionView", lightDirectionView);
    waterShader_.setVec3("shallowColor", palette.shallowColor);
    waterShader_.setVec3("deepColor", palette.deepColor);
    waterShader_.setVec3("foamColor", palette.foamColor);
    waterShader_.setVec3("skyColor", palette.skyColor);
    waterShader_.setFloat("waterAlpha", renderSettings.waterAlpha);
    waterShader_.setFloat("particleWorldRadius", particleWorldRadius_ * renderSettings.particleScale);
    waterShader_.setFloat("viewportHeight", static_cast<float>(viewportHeight));
    waterShader_.setFloat("ambientStrength", renderSettings.ambientStrength);
    waterShader_.setFloat("specularStrength", renderSettings.specularStrength);
    waterShader_.setFloat("fresnelStrength", renderSettings.fresnelStrength);
    waterShader_.setInt("debugMode", debugMode);
    glBindVertexArray(particleVao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particleCount_));

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    lineShader_.use();
    lineShader_.setMat4("model", glm::mat4(1.0f));
    lineShader_.setMat4("view", view);
    lineShader_.setMat4("projection", projection);
    lineShader_.setVec3("lineColor", palette.accentColor);
    glBindVertexArray(lineVao_);
    glDrawElements(GL_LINES, lineIndexCount_, GL_UNSIGNED_INT, nullptr);

    if (interactionVisible_ && debugMode == 4 && !interactionVertices_.empty()) {
        const glm::vec3 highlightColor =
            glm::mix(palette.accentColor, palette.foamColor, glm::clamp(interactionIntensity_, 0.0f, 1.0f));
        lineShader_.use();
        lineShader_.setMat4("model", glm::mat4(1.0f));
        lineShader_.setMat4("view", view);
        lineShader_.setMat4("projection", projection);
        lineShader_.setVec3("lineColor", highlightColor);
        glBindVertexArray(interactionVao_);
        glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(interactionVertices_.size()));
    }
}

void WaterRenderer::ensureParticleCapacity(std::size_t particleCount) {
    if (particleCount <= particleCapacity_ && particleVao_ != 0 && particleVbo_ != 0) {
        return;
    }

    particleCapacity_ = std::max<std::size_t>(particleCount, 1);

    if (particleVao_ == 0) {
        glGenVertexArrays(1, &particleVao_);
    }
    if (particleVbo_ == 0) {
        glGenBuffers(1, &particleVbo_);
    }

    glBindVertexArray(particleVao_);
    glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(particleCapacity_ * sizeof(ParticleVertex)),
        nullptr,
        GL_STREAM_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(ParticleVertex),
        reinterpret_cast<void*>(offsetof(ParticleVertex, position))
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(ParticleVertex),
        reinterpret_cast<void*>(offsetof(ParticleVertex, metrics))
    );
    glEnableVertexAttribArray(1);
}

void WaterRenderer::buildBasinMesh(float halfExtent, float floorY, float lipY) {
    basinVertices_ = {
        {{-halfExtent, floorY, -halfExtent}, {0.0f, 1.0f, 0.0f}},
        {{halfExtent, floorY, -halfExtent}, {0.0f, 1.0f, 0.0f}},
        {{halfExtent, floorY, halfExtent}, {0.0f, 1.0f, 0.0f}},
        {{-halfExtent, floorY, -halfExtent}, {0.0f, 1.0f, 0.0f}},
        {{halfExtent, floorY, halfExtent}, {0.0f, 1.0f, 0.0f}},
        {{-halfExtent, floorY, halfExtent}, {0.0f, 1.0f, 0.0f}},

        {{-halfExtent, floorY, halfExtent}, {0.0f, 0.0f, -1.0f}},
        {{halfExtent, floorY, halfExtent}, {0.0f, 0.0f, -1.0f}},
        {{halfExtent, lipY, halfExtent}, {0.0f, 0.0f, -1.0f}},
        {{-halfExtent, floorY, halfExtent}, {0.0f, 0.0f, -1.0f}},
        {{halfExtent, lipY, halfExtent}, {0.0f, 0.0f, -1.0f}},
        {{-halfExtent, lipY, halfExtent}, {0.0f, 0.0f, -1.0f}},

        {{halfExtent, floorY, halfExtent}, {-1.0f, 0.0f, 0.0f}},
        {{halfExtent, floorY, -halfExtent}, {-1.0f, 0.0f, 0.0f}},
        {{halfExtent, lipY, -halfExtent}, {-1.0f, 0.0f, 0.0f}},
        {{halfExtent, floorY, halfExtent}, {-1.0f, 0.0f, 0.0f}},
        {{halfExtent, lipY, -halfExtent}, {-1.0f, 0.0f, 0.0f}},
        {{halfExtent, lipY, halfExtent}, {-1.0f, 0.0f, 0.0f}},

        {{halfExtent, floorY, -halfExtent}, {0.0f, 0.0f, 1.0f}},
        {{-halfExtent, floorY, -halfExtent}, {0.0f, 0.0f, 1.0f}},
        {{-halfExtent, lipY, -halfExtent}, {0.0f, 0.0f, 1.0f}},
        {{halfExtent, floorY, -halfExtent}, {0.0f, 0.0f, 1.0f}},
        {{-halfExtent, lipY, -halfExtent}, {0.0f, 0.0f, 1.0f}},
        {{halfExtent, lipY, -halfExtent}, {0.0f, 0.0f, 1.0f}},

        {{-halfExtent, floorY, -halfExtent}, {1.0f, 0.0f, 0.0f}},
        {{-halfExtent, floorY, halfExtent}, {1.0f, 0.0f, 0.0f}},
        {{-halfExtent, lipY, halfExtent}, {1.0f, 0.0f, 0.0f}},
        {{-halfExtent, floorY, -halfExtent}, {1.0f, 0.0f, 0.0f}},
        {{-halfExtent, lipY, halfExtent}, {1.0f, 0.0f, 0.0f}},
        {{-halfExtent, lipY, -halfExtent}, {1.0f, 0.0f, 0.0f}},
    };

    lineVertices_ = {
        {-halfExtent, floorY, -halfExtent},
        {halfExtent, floorY, -halfExtent},
        {halfExtent, floorY, halfExtent},
        {-halfExtent, floorY, halfExtent},
        {-halfExtent, lipY, -halfExtent},
        {halfExtent, lipY, -halfExtent},
        {halfExtent, lipY, halfExtent},
        {-halfExtent, lipY, halfExtent}
    };

    lineIndices_ = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };
    lineIndexCount_ = static_cast<int>(lineIndices_.size());

    if (basinVao_ == 0) {
        glGenVertexArrays(1, &basinVao_);
        glGenBuffers(1, &basinVbo_);
    }
    glBindVertexArray(basinVao_);
    glBindBuffer(GL_ARRAY_BUFFER, basinVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(basinVertices_.size() * sizeof(SolidVertex)),
        basinVertices_.data(),
        GL_STATIC_DRAW
    );
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SolidVertex), reinterpret_cast<void*>(offsetof(SolidVertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SolidVertex), reinterpret_cast<void*>(offsetof(SolidVertex, normal)));
    glEnableVertexAttribArray(1);

    if (lineVao_ == 0) {
        glGenVertexArrays(1, &lineVao_);
        glGenBuffers(1, &lineVbo_);
        glGenBuffers(1, &lineEbo_);
    }
    glBindVertexArray(lineVao_);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(lineVertices_.size() * sizeof(glm::vec3)),
        lineVertices_.data(),
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lineEbo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(lineIndices_.size() * sizeof(unsigned int)),
        lineIndices_.data(),
        GL_STATIC_DRAW
    );
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    if (interactionVao_ == 0) {
        glGenVertexArrays(1, &interactionVao_);
        glGenBuffers(1, &interactionVbo_);
    }
    glBindVertexArray(interactionVao_);
    glBindBuffer(GL_ARRAY_BUFFER, interactionVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>((kInteractionSegments + 1) * sizeof(glm::vec3)),
        nullptr,
        GL_DYNAMIC_DRAW
    );
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
}

void WaterRenderer::updateInteractionRing() {
    interactionVertices_.clear();
    if (!interactionVisible_ || interactionRadius_ <= 0.0f) {
        return;
    }

    interactionVertices_.reserve(kInteractionSegments);
    for (int segment = 0; segment < kInteractionSegments; ++segment) {
        const float angle =
            (static_cast<float>(segment) / static_cast<float>(kInteractionSegments)) * glm::two_pi<float>();
        interactionVertices_.push_back(
            interactionCenter_ +
            glm::vec3(std::cos(angle) * interactionRadius_, 0.015f, std::sin(angle) * interactionRadius_)
        );
    }

    glBindBuffer(GL_ARRAY_BUFFER, interactionVbo_);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(interactionVertices_.size() * sizeof(glm::vec3)),
        interactionVertices_.data()
    );
}

void WaterRenderer::clear() {
    if (particleVbo_ != 0) {
        glDeleteBuffers(1, &particleVbo_);
        particleVbo_ = 0;
    }
    if (particleVao_ != 0) {
        glDeleteVertexArrays(1, &particleVao_);
        particleVao_ = 0;
    }
    if (basinVbo_ != 0) {
        glDeleteBuffers(1, &basinVbo_);
        basinVbo_ = 0;
    }
    if (basinVao_ != 0) {
        glDeleteVertexArrays(1, &basinVao_);
        basinVao_ = 0;
    }
    if (lineEbo_ != 0) {
        glDeleteBuffers(1, &lineEbo_);
        lineEbo_ = 0;
    }
    if (lineVbo_ != 0) {
        glDeleteBuffers(1, &lineVbo_);
        lineVbo_ = 0;
    }
    if (lineVao_ != 0) {
        glDeleteVertexArrays(1, &lineVao_);
        lineVao_ = 0;
    }
    if (interactionVbo_ != 0) {
        glDeleteBuffers(1, &interactionVbo_);
        interactionVbo_ = 0;
    }
    if (interactionVao_ != 0) {
        glDeleteVertexArrays(1, &interactionVao_);
        interactionVao_ = 0;
    }
}
