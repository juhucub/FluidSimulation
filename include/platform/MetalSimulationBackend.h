#pragma once

#include <cstddef>

#include <glm/glm.hpp>

#include "sim/WaterSimulation.h"

class MetalSimulationBackend {
public:
    MetalSimulationBackend();
    ~MetalSimulationBackend();

    bool available() const;
    void reset(const WaterSimulationSettings& settings);
    void updateSettings(const WaterSimulationSettings& settings, bool structuralReset);
    void step(float deltaTime);
    void addImpulse(const glm::vec2& worldXZ, const glm::vec2& dragVelocity, float heightDelta, float radius);
    void buildRenderSnapshot(WaterRenderSnapshot& snapshot) const;

    const SimulationStats& stats() const;
    float interactionPlaneY() const;
    std::size_t particleCount() const;

private:
    struct Impl;
    Impl* impl_;
};
