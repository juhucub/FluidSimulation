#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

enum class SimulationBackend {
    CPU = 0,
    Metal = 1,
};

enum class SimulationQualityPolicy {
    Balanced = 0,
    FavorResponsiveness = 1,
    FavorCorrectness = 2,
    Auto = 3,
};

struct WaterSimulationSettings {
    int particlesX = 24;
    int particlesY = 14;
    int particlesZ = 14;
    float domainSize = 6.0f;
    float containerFloorY = -1.15f;
    float containerLipY = 0.2f;
    float particleRadius = 0.065f;
    float smoothingLength = 0.225f;
    float particleMass = 0.0135f;
    float restDensity = 4.9f;
    float pressureStiffness = 0.052f;
    float nearPressureStiffness = 0.055f;
    float viscosityLinear = 0.22f;
    float viscosityQuadratic = 0.06f;
    float xsphViscosity = 0.16f;
    float velocityDamping = 0.58f;
    float restVelocityDamping = 2.35f;
    float velocityTransfer = 0.62f;
    float positionRelaxation = 0.38f;
    float tensileCorrection = 0.00018f;
    float boundaryDamping = 0.65f;
    float boundaryRestitution = 0.025f;
    float boundaryFriction = 0.16f;
    float gravity = 12.5f;
    int substeps = 3;
    int pressureIterations = 5;
    float cflFactor = 0.32f;
    int maxSubsteps = 8;
    float maxSpeed = 4.6f;
    float interactionRadius = 0.42f;
    float interactionForce = 1.2f;
    float interactionLift = 0.55f;
    float interactionMaxSpeed = 2.4f;
    float foamDecay = 1.8f;
    SimulationBackend backend = SimulationBackend::CPU;
    SimulationQualityPolicy qualityPolicy = SimulationQualityPolicy::Balanced;
};

struct SimulationStats {
    std::uint64_t frameIndex = 0;
    std::size_t particleCount = 0;
    std::size_t activeCells = 0;
    std::size_t neighborSamples = 0;
    float gridBuildMs = 0.0f;
    float densityPassMs = 0.0f;
    float constraintPassMs = 0.0f;
    float integrateMs = 0.0f;
    float finalizeMs = 0.0f;
    float snapshotMs = 0.0f;
    float totalStepMs = 0.0f;
    float simulatedDeltaTime = 0.0f;
    int executedSubsteps = 0;
    int executedSolverIterations = 0;
    float averageDensity = 0.0f;
    float averageDensityError = 0.0f;
    float maxSpeed = 0.0f;
};

struct WaterRenderParticle {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec4 metrics = glm::vec4(0.0f);
};

struct WaterRenderSnapshot {
    std::vector<WaterRenderParticle> particles;
    float particleRadius = 0.07f;
    float interactionPlaneY = 0.0f;
    float halfDomainSize = 0.0f;
    float containerFloorY = 0.0f;
    float containerLipY = 0.0f;
    std::uint64_t version = 0;
};

class MetalSimulationBackend;

class WaterSimulation {
public:
    explicit WaterSimulation(const WaterSimulationSettings& settings = {});
    ~WaterSimulation();

    void reset();
    void step(float deltaTime);
    void addImpulse(const glm::vec2& worldXZ, const glm::vec2& dragVelocity, float heightDelta, float radius);
    void buildRenderSnapshot(WaterRenderSnapshot& snapshot) const;

    WaterSimulationSettings& settings();
    const WaterSimulationSettings& settings() const;
    const SimulationStats& stats() const;

    float domainSize() const;
    float halfDomainSize() const;
    float cellSize() const;
    float containerFloorY() const;
    float containerLipY() const;
    float interactionPlaneY() const;
    float particleRadius() const;
    float restDensity() const;
    std::size_t particleCount() const;

private:
    struct WorkerPool;

    void sanitizeSettings();
    void updateConfiguration();
    void rebuildSpatialGrid(const std::vector<float>& xs, const std::vector<float>& ys, const std::vector<float>& zs);
    void integratePredictedPositions(float deltaTime);
    void computeDensityAndLambdas(std::size_t& neighborSamples);
    void computePositionCorrections(float deltaTime);
    void applyPredictedCorrections();
    void computeFinalState(float deltaTime, std::size_t& neighborSamples);
    void enforceBounds(float& x, float& y, float& z) const;
    void updateDerivedState();

    int positionToCellIndex(float x, float y, float z) const;
    glm::ivec3 positionToCell(float x, float y, float z) const;

    template <typename Func>
    void forEachNeighbor(
        std::size_t particleIndex,
        const std::vector<float>& xs,
        const std::vector<float>& ys,
        const std::vector<float>& zs,
        Func&& func
    ) const;

    WaterSimulationSettings settings_;
    SimulationStats stats_;
    float halfDomain_;
    float cellSize_;
    glm::vec3 gridMin_;
    glm::vec3 gridMax_;
    int gridResolutionX_;
    int gridResolutionY_;
    int gridResolutionZ_;
    float interactionPlaneY_;
    float spawnTopY_;

    std::vector<float> posX_;
    std::vector<float> posY_;
    std::vector<float> posZ_;
    std::vector<float> predX_;
    std::vector<float> predY_;
    std::vector<float> predZ_;
    std::vector<float> velX_;
    std::vector<float> velY_;
    std::vector<float> velZ_;
    std::vector<float> density_;
    std::vector<float> lambda_;
    std::vector<float> deltaX_;
    std::vector<float> deltaY_;
    std::vector<float> deltaZ_;
    std::vector<float> speedMetric_;
    std::vector<float> densityMetric_;
    std::vector<float> pressureMetric_;
    std::vector<float> interactionMetric_;
    std::vector<float> foam_;
    std::vector<float> interactionHeat_;
    std::vector<int> particleCellIndices_;
    std::vector<int> cellCounts_;
    std::vector<int> cellStarts_;
    std::vector<int> sortedParticleIndices_;
    WorkerPool* workerPool_;
    MetalSimulationBackend* metalBackend_;
    SimulationBackend activeBackend_;
};

template <typename Func>
void WaterSimulation::forEachNeighbor(
    std::size_t particleIndex,
    const std::vector<float>& xs,
    const std::vector<float>& ys,
    const std::vector<float>& zs,
    Func&& func
) const {
    const glm::ivec3 baseCell = positionToCell(xs[particleIndex], ys[particleIndex], zs[particleIndex]);

    for (int z = baseCell.z - 1; z <= baseCell.z + 1; ++z) {
        if (z < 0 || z >= gridResolutionZ_) {
            continue;
        }
        for (int y = baseCell.y - 1; y <= baseCell.y + 1; ++y) {
            if (y < 0 || y >= gridResolutionY_) {
                continue;
            }
            for (int x = baseCell.x - 1; x <= baseCell.x + 1; ++x) {
                if (x < 0 || x >= gridResolutionX_) {
                    continue;
                }

                const int cellIndex = (z * gridResolutionY_ + y) * gridResolutionX_ + x;
                for (int entry = cellStarts_[static_cast<std::size_t>(cellIndex)];
                     entry < cellStarts_[static_cast<std::size_t>(cellIndex) + 1];
                     ++entry) {
                    const std::size_t neighborIndex =
                        static_cast<std::size_t>(sortedParticleIndices_[static_cast<std::size_t>(entry)]);
                    if (neighborIndex == particleIndex) {
                        continue;
                    }

                    const float dx = xs[particleIndex] - xs[neighborIndex];
                    const float dy = ys[particleIndex] - ys[neighborIndex];
                    const float dz = zs[particleIndex] - zs[neighborIndex];
                    const float distanceSquared = dx * dx + dy * dy + dz * dz;
                    if (distanceSquared >= cellSize_ * cellSize_) {
                        continue;
                    }

                    func(neighborIndex, dx, dy, dz, distanceSquared);
                }
            }
        }
    }
}
