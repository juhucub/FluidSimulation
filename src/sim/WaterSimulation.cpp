#include "sim/WaterSimulation.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#ifdef __APPLE__
#include "platform/MetalSimulationBackend.h"
#endif

namespace {
using Clock = std::chrono::steady_clock;

constexpr float kEpsilon = 1.0e-6f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kSpawnSpacingScale = 1.95f;
constexpr float kMinDeltaTime = 1.0e-4f;
constexpr float kMaxDeltaTime = 1.0f / 24.0f;
constexpr float kQuietSpeedThreshold = 0.45f;
constexpr float kQuietDensityErrorThreshold = 0.08f;

float toMilliseconds(const Clock::duration& duration) {
    return std::chrono::duration<float, std::milli>(duration).count();
}

float computePoly6Coefficient(float smoothingLength) {
    const float h2 = smoothingLength * smoothingLength;
    const float h4 = h2 * h2;
    const float h6 = h4 * h2;
    const float h9 = h6 * h2 * smoothingLength;
    return 315.0f / (64.0f * kPi * h9);
}

float computeSpikyGradientCoefficient(float smoothingLength) {
    const float h2 = smoothingLength * smoothingLength;
    const float h4 = h2 * h2;
    const float h6 = h4 * h2;
    return -45.0f / (kPi * h6);
}

float poly6Kernel(float distanceSquared, float smoothingLengthSquared, float poly6Coefficient) {
    if (distanceSquared >= smoothingLengthSquared) {
        return 0.0f;
    }

    const float h2MinusR2 = smoothingLengthSquared - distanceSquared;
    return poly6Coefficient * h2MinusR2 * h2MinusR2 * h2MinusR2;
}

glm::vec3 spikyGradient(
    const glm::vec3& delta,
    float distance,
    float smoothingLength,
    float spikyGradientCoefficient
) {
    if (distance <= kEpsilon || distance >= smoothingLength) {
        return glm::vec3(0.0f);
    }

    const float scale =
        spikyGradientCoefficient * (smoothingLength - distance) * (smoothingLength - distance) / distance;
    return delta * scale;
}

float smoothPulse(float value) {
    const float clamped = glm::clamp(value, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

float structuralDifference(const WaterSimulationSettings& lhs, const WaterSimulationSettings& rhs) {
    return std::abs(lhs.domainSize - rhs.domainSize) +
           std::abs(lhs.containerFloorY - rhs.containerFloorY) +
           std::abs(lhs.containerLipY - rhs.containerLipY) +
           std::abs(lhs.particleRadius - rhs.particleRadius) +
           std::abs(lhs.smoothingLength - rhs.smoothingLength) +
           std::abs(lhs.particleMass - rhs.particleMass) +
           std::abs(lhs.restDensity - rhs.restDensity);
}

float computeOpenTopGridMaxY(const WaterSimulationSettings& settings) {
    const float spacing = settings.particleRadius * kSpawnSpacingScale;
    const float estimatedSpawnTop =
        settings.containerFloorY +
        settings.particleRadius * 1.6f +
        static_cast<float>(std::max(settings.particlesY - 1, 0)) * spacing +
        settings.particleRadius;
    const float splashHeadroom = std::max(settings.smoothingLength * 4.0f, settings.particleRadius * 8.0f);
    return std::max(settings.containerLipY + splashHeadroom, estimatedSpawnTop + splashHeadroom * 0.5f);
}
} // namespace

struct WaterSimulation::WorkerPool {
    explicit WorkerPool(std::size_t threadCount) {
        if (threadCount <= 1) {
            return;
        }

        const std::size_t workerCount = threadCount - 1;
        threads_.reserve(workerCount);
        for (std::size_t i = 0; i < workerCount; ++i) {
            threads_.emplace_back([this] { workerLoop(); });
        }
    }

    ~WorkerPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
            ++generation_;
        }
        condition_.notify_all();
        for (std::thread& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void parallelFor(std::size_t itemCount, const std::function<void(std::size_t, std::size_t)>& task) {
        if (itemCount == 0) {
            return;
        }

        if (threads_.empty() || itemCount < 512) {
            task(0, itemCount);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            task_ = task;
            itemCount_ = itemCount;
            nextIndex_.store(0, std::memory_order_relaxed);
            const std::size_t participantCount = threads_.size() + 1;
            chunkSize_ = std::max<std::size_t>(64, (itemCount + participantCount * 4 - 1) / (participantCount * 4));
            activeParticipants_ = participantCount;
            taskRunning_ = true;
            ++generation_;
        }

        condition_.notify_all();
        executeTask();

        std::unique_lock<std::mutex> lock(mutex_);
        finishParticipant(lock);
        doneCondition_.wait(lock, [this] { return !taskRunning_; });
        task_ = {};
    }

private:
    void workerLoop() {
        std::uint64_t observedGeneration = 0;
        std::unique_lock<std::mutex> lock(mutex_);
        while (true) {
            condition_.wait(lock, [this, &observedGeneration] {
                return stop_ || generation_ != observedGeneration;
            });

            if (stop_) {
                return;
            }

            observedGeneration = generation_;
            lock.unlock();
            executeTask();
            lock.lock();
            finishParticipant(lock);
        }
    }

    void executeTask() {
        while (true) {
            const std::size_t begin = nextIndex_.fetch_add(chunkSize_, std::memory_order_relaxed);
            if (begin >= itemCount_) {
                break;
            }

            const std::size_t end = std::min(begin + chunkSize_, itemCount_);
            task_(begin, end);
        }
    }

    void finishParticipant(std::unique_lock<std::mutex>& lock) {
        if (activeParticipants_ > 0) {
            --activeParticipants_;
        }

        if (activeParticipants_ == 0) {
            taskRunning_ = false;
            lock.unlock();
            doneCondition_.notify_all();
            lock.lock();
        }
    }

    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::condition_variable doneCondition_;
    std::function<void(std::size_t, std::size_t)> task_;
    std::atomic<std::size_t> nextIndex_{0};
    std::size_t itemCount_ = 0;
    std::size_t chunkSize_ = 1;
    std::size_t activeParticipants_ = 0;
    bool taskRunning_ = false;
    bool stop_ = false;
    std::uint64_t generation_ = 0;
};

void WaterSimulation::ParticleSoA::resize(std::size_t particleCount, float restDensity) {
    posX.assign(particleCount, 0.0f);
    posY.assign(particleCount, 0.0f);
    posZ.assign(particleCount, 0.0f);
    predX.assign(particleCount, 0.0f);
    predY.assign(particleCount, 0.0f);
    predZ.assign(particleCount, 0.0f);
    velX.assign(particleCount, 0.0f);
    velY.assign(particleCount, 0.0f);
    velZ.assign(particleCount, 0.0f);
    provisionalVelX.assign(particleCount, 0.0f);
    provisionalVelY.assign(particleCount, 0.0f);
    provisionalVelZ.assign(particleCount, 0.0f);
    density.assign(particleCount, restDensity);
    alpha.assign(particleCount, 0.0f);
    densityPressure.assign(particleCount, 0.0f);
    divergencePressure.assign(particleCount, 0.0f);
    densityError.assign(particleCount, 0.0f);
    divergenceError.assign(particleCount, 0.0f);
    deltaX.assign(particleCount, 0.0f);
    deltaY.assign(particleCount, 0.0f);
    deltaZ.assign(particleCount, 0.0f);
    speedMetric.assign(particleCount, 0.0f);
    densityMetric.assign(particleCount, 1.0f);
    pressureMetric.assign(particleCount, 0.0f);
    interactionMetric.assign(particleCount, 0.0f);
    foam.assign(particleCount, 0.0f);
    interactionHeat.assign(particleCount, 0.0f);
    stableParticleIds.assign(particleCount, 0);
    cellIndices.assign(particleCount, 0);
}

WaterSimulation::WaterSimulation(const WaterSimulationSettings& settings)
    : settings_(settings),
      halfDomain_(0.0f),
      cellSize_(0.0f),
      gridMin_(0.0f),
      gridMax_(0.0f),
      gridResolutionX_(0),
      gridResolutionY_(0),
      gridResolutionZ_(0),
      interactionPlaneY_(0.0f),
      spawnTopY_(0.0f),
      smoothingLengthSquared_(0.0f),
      poly6Coefficient_(0.0f),
      spikyGradientCoefficient_(0.0f),
      selfDensityKernel_(0.0f),
      correctionReferenceKernel_(0.0f),
      workerPool_(new WorkerPool(std::max(1u, std::thread::hardware_concurrency()))),
      metalBackend_(nullptr),
      activeBackend_(SimulationBackend::CPU) {
#ifdef __APPLE__
    metalBackend_ = new MetalSimulationBackend();
#endif
    reset();
}

WaterSimulation::~WaterSimulation() {
    delete metalBackend_;
    delete workerPool_;
}

void WaterSimulation::sanitizeSettings() {
    settings_.particlesX = std::max(settings_.particlesX, 4);
    settings_.particlesY = std::max(settings_.particlesY, 2);
    settings_.particlesZ = std::max(settings_.particlesZ, 4);
    settings_.domainSize = std::max(settings_.domainSize, 2.0f);
    settings_.particleRadius = std::max(settings_.particleRadius, 0.02f);
    settings_.smoothingLength = std::max(settings_.smoothingLength, settings_.particleRadius * 2.4f);
    settings_.particleMass = std::max(settings_.particleMass, 0.001f);
    settings_.restDensity = std::max(settings_.restDensity, 0.5f);
    settings_.pressureStiffness = std::max(settings_.pressureStiffness, 0.001f);
    settings_.nearPressureStiffness = std::max(settings_.nearPressureStiffness, 0.0f);
    settings_.viscosityLinear = std::max(settings_.viscosityLinear, 0.0f);
    settings_.viscosityQuadratic = std::max(settings_.viscosityQuadratic, 0.0f);
    settings_.xsphViscosity = glm::clamp(settings_.xsphViscosity, 0.0f, 1.0f);
    settings_.velocityDamping = std::max(settings_.velocityDamping, 0.0f);
    settings_.restVelocityDamping = std::max(settings_.restVelocityDamping, 0.0f);
    settings_.velocityTransfer = glm::clamp(settings_.velocityTransfer, 0.0f, 1.0f);
    settings_.positionRelaxation = glm::clamp(settings_.positionRelaxation, 0.05f, 1.0f);
    settings_.tensileCorrection = glm::clamp(settings_.tensileCorrection, 0.0f, 0.01f);
    settings_.boundaryDamping = glm::clamp(settings_.boundaryDamping, 0.0f, 1.0f);
    settings_.boundaryRestitution = glm::clamp(settings_.boundaryRestitution, 0.0f, 0.4f);
    settings_.boundaryFriction = glm::clamp(settings_.boundaryFriction, 0.0f, 1.0f);
    settings_.substeps = std::max(settings_.substeps, 1);
    settings_.pressureIterations = std::max(settings_.pressureIterations, 1);
    settings_.cflFactor = glm::clamp(settings_.cflFactor, 0.1f, 0.95f);
    settings_.maxSubsteps = std::max(settings_.maxSubsteps, settings_.substeps);
    settings_.maxSpeed = std::max(settings_.maxSpeed, 0.5f);
    settings_.interactionRadius = std::max(settings_.interactionRadius, settings_.particleRadius * 2.5f);
    settings_.interactionMaxSpeed = std::max(settings_.interactionMaxSpeed, 0.5f);
}

void WaterSimulation::updateConfiguration() {
    halfDomain_ = settings_.domainSize * 0.5f;
    cellSize_ = settings_.smoothingLength;
    smoothingLengthSquared_ = settings_.smoothingLength * settings_.smoothingLength;
    poly6Coefficient_ = computePoly6Coefficient(settings_.smoothingLength);
    spikyGradientCoefficient_ = computeSpikyGradientCoefficient(settings_.smoothingLength);
    selfDensityKernel_ = poly6Kernel(0.0f, smoothingLengthSquared_, poly6Coefficient_);
    gridMin_ = glm::vec3(-halfDomain_, settings_.containerFloorY, -halfDomain_);
    gridMax_ = glm::vec3(halfDomain_, computeOpenTopGridMaxY(settings_), halfDomain_);
    correctionReferenceKernel_ = std::max(
        poly6Kernel(smoothingLengthSquared_ * 0.12f, smoothingLengthSquared_, poly6Coefficient_),
        kEpsilon
    );

    gridResolutionX_ = std::max(1, static_cast<int>(std::ceil((gridMax_.x - gridMin_.x) / cellSize_)) + 1);
    gridResolutionY_ = std::max(1, static_cast<int>(std::ceil((gridMax_.y - gridMin_.y) / cellSize_)) + 1);
    gridResolutionZ_ = std::max(1, static_cast<int>(std::ceil((gridMax_.z - gridMin_.z) / cellSize_)) + 1);

    const std::size_t particleCount =
        static_cast<std::size_t>(settings_.particlesX) *
        static_cast<std::size_t>(settings_.particlesY) *
        static_cast<std::size_t>(settings_.particlesZ);
    const std::size_t cellCount =
        static_cast<std::size_t>(gridResolutionX_) *
        static_cast<std::size_t>(gridResolutionY_) *
        static_cast<std::size_t>(gridResolutionZ_);

    activeState_.resize(particleCount, settings_.restDensity);
    scratchState_.resize(particleCount, settings_.restDensity);
    cellCounts_.assign(cellCount, 0);
    cellStarts_.assign(cellCount + 1, 0);
    cellWriteHeads_.assign(cellCount, 0);
}

void WaterSimulation::reset() {
    sanitizeSettings();
    updateConfiguration();

    if (settings_.backend == SimulationBackend::Metal && metalBackend_ != nullptr && metalBackend_->available()) {
        activeBackend_ = SimulationBackend::Metal;
        metalBackend_->reset(settings_);
        stats_ = metalBackend_->stats();
        interactionPlaneY_ = metalBackend_->interactionPlaneY();
        return;
    }

    activeBackend_ = SimulationBackend::CPU;

    const float spacing = settings_.particleRadius * kSpawnSpacingScale;
    const float startX = -0.5f * static_cast<float>(settings_.particlesX - 1) * spacing;
    const float startZ = -0.5f * static_cast<float>(settings_.particlesZ - 1) * spacing;
    const float startY = settings_.containerFloorY + settings_.particleRadius * 1.6f;
    auto& state = activeState_;

    std::size_t index = 0;
    for (int y = 0; y < settings_.particlesY; ++y) {
        for (int z = 0; z < settings_.particlesZ; ++z) {
            for (int x = 0; x < settings_.particlesX; ++x) {
                const float jitterA = std::sin(static_cast<float>(index) * 1.6180339f);
                const float jitterB = std::cos(static_cast<float>(index) * 2.4142136f);
                float px =
                    startX + static_cast<float>(x) * spacing + jitterA * settings_.particleRadius * 0.02f;
                float py =
                    startY + static_cast<float>(y) * spacing + jitterB * settings_.particleRadius * 0.015f;
                float pz =
                    startZ + static_cast<float>(z) * spacing + jitterB * settings_.particleRadius * 0.02f;
                enforceBounds(px, py, pz);

                state.posX[index] = px;
                state.posY[index] = py;
                state.posZ[index] = pz;
                state.predX[index] = px;
                state.predY[index] = py;
                state.predZ[index] = pz;
                state.stableParticleIds[index] = static_cast<int>(index);
                ++index;
            }
        }
    }

    spawnTopY_ = startY + static_cast<float>(settings_.particlesY - 1) * spacing + settings_.particleRadius;
    rebuildSpatialGrid(state.posX, state.posY, state.posZ);
    constexpr int kSettleSteps = 10;
    constexpr float kSettleDeltaTime = 1.0f / 240.0f;
    for (int settleStep = 0; settleStep < kSettleSteps; ++settleStep) {
        step(kSettleDeltaTime);
    }
    rebuildSpatialGrid(state.posX, state.posY, state.posZ);
    updateDerivedState();
    stats_ = {};
    stats_.particleCount = particleCount();
}

void WaterSimulation::rebuildSpatialGrid(
    const std::vector<float>& xs,
    const std::vector<float>& ys,
    const std::vector<float>& zs
) {
    auto& state = activeState_;
    auto& scratch = scratchState_;
    std::fill(cellCounts_.begin(), cellCounts_.end(), 0);

    for (std::size_t i = 0; i < xs.size(); ++i) {
        const int cellIndex = positionToCellIndex(xs[i], ys[i], zs[i]);
        state.cellIndices[i] = cellIndex;
        ++cellCounts_[cellIndex];
    }

    stats_.activeCells = 0;
    cellStarts_[0] = 0;
    for (std::size_t cellIndex = 0; cellIndex < cellCounts_.size(); ++cellIndex) {
        if (cellCounts_[cellIndex] > 0) {
            ++stats_.activeCells;
        }
        cellStarts_[cellIndex + 1] = cellStarts_[cellIndex] + cellCounts_[cellIndex];
    }

    std::copy(cellStarts_.begin(), cellStarts_.end() - 1, cellWriteHeads_.begin());
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const std::size_t destination = static_cast<std::size_t>(cellWriteHeads_[state.cellIndices[i]]++);
        scratch.cellIndices[destination] = state.cellIndices[i];
        scratch.stableParticleIds[destination] = state.stableParticleIds[i];
        scratch.posX[destination] = state.posX[i];
        scratch.posY[destination] = state.posY[i];
        scratch.posZ[destination] = state.posZ[i];
        scratch.predX[destination] = state.predX[i];
        scratch.predY[destination] = state.predY[i];
        scratch.predZ[destination] = state.predZ[i];
        scratch.velX[destination] = state.velX[i];
        scratch.velY[destination] = state.velY[i];
        scratch.velZ[destination] = state.velZ[i];
        scratch.densityPressure[destination] = state.densityPressure[i];
        scratch.foam[destination] = state.foam[i];
        scratch.interactionHeat[destination] = state.interactionHeat[i];
    }

    std::swap(activeState_, scratchState_);
}

void WaterSimulation::integratePredictedPositions(float deltaTime) {
    const float damping = std::exp(-settings_.velocityDamping * deltaTime);
    auto& state = activeState_;
    workerPool_->parallelFor(state.posX.size(), [this, &state, deltaTime, damping](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            state.velY[i] -= settings_.gravity * deltaTime;
            state.velX[i] *= damping;
            state.velY[i] *= damping;
            state.velZ[i] *= damping;

            const float speedSquared =
                state.velX[i] * state.velX[i] + state.velY[i] * state.velY[i] + state.velZ[i] * state.velZ[i];
            const float maxSpeedSquared = settings_.maxSpeed * settings_.maxSpeed;
            if (speedSquared > maxSpeedSquared) {
                const float scale = settings_.maxSpeed / std::sqrt(std::max(speedSquared, kEpsilon));
                state.velX[i] *= scale;
                state.velY[i] *= scale;
                state.velZ[i] *= scale;
            }

            float px = state.posX[i] + state.velX[i] * deltaTime;
            float py = state.posY[i] + state.velY[i] * deltaTime;
            float pz = state.posZ[i] + state.velZ[i] * deltaTime;
            enforceBounds(px, py, pz);
            state.predX[i] = px;
            state.predY[i] = py;
            state.predZ[i] = pz;
        }
    });
}

void WaterSimulation::computeDensityAndAlpha(std::size_t& neighborSamples) {
    auto& state = activeState_;
    const float restDensity = std::max(settings_.restDensity, kEpsilon);
    const float alphaEpsilon = 4.0e-3f;
    const float densityGradientScale = settings_.particleMass / restDensity;
    std::atomic<std::size_t> neighborCounter{0};

    workerPool_->parallelFor(
        state.predX.size(),
        [this, &state, restDensity, alphaEpsilon, densityGradientScale, &neighborCounter](std::size_t begin, std::size_t end) {
            std::size_t localNeighbors = 0;
            for (std::size_t i = begin; i < end; ++i) {
                float density = settings_.particleMass * selfDensityKernel_;
                glm::vec3 gradI(0.0f);
                float sumGradients = 0.0f;

                forEachNeighbor(
                    i,
                    state,
                    state.predX,
                    state.predY,
                    state.predZ,
                    [&](std::size_t, float dx, float dy, float dz, float distanceSquared) {
                        const float kernel = poly6Kernel(distanceSquared, smoothingLengthSquared_, poly6Coefficient_);
                        density += settings_.particleMass * kernel;

                        const float distance = std::sqrt(std::max(distanceSquared, kEpsilon));
                        const glm::vec3 gradient =
                            densityGradientScale *
                            spikyGradient(
                                glm::vec3(dx, dy, dz),
                                distance,
                                settings_.smoothingLength,
                                spikyGradientCoefficient_
                            );
                        sumGradients += glm::dot(gradient, gradient);
                        gradI += gradient;
                        ++localNeighbors;
                    }
                );

                sumGradients += glm::dot(gradI, gradI);
                state.density[i] = density;
                state.alpha[i] = 1.0f / (sumGradients + alphaEpsilon);
                state.densityError[i] = density / restDensity - 1.0f;
            }
            neighborCounter.fetch_add(localNeighbors, std::memory_order_relaxed);
        }
    );

    neighborSamples += neighborCounter.load(std::memory_order_relaxed);
}

void WaterSimulation::solveDensityPressureIteration() {
    auto& state = activeState_;
    const float restDensity = std::max(settings_.restDensity, kEpsilon);
    const float targetSeparation = settings_.particleRadius * 1.45f;
    const float maxCorrection = settings_.particleRadius * 0.2f;
    const float correctionScale = settings_.positionRelaxation * settings_.pressureStiffness / restDensity;
    const float tensileScale = settings_.tensileCorrection * settings_.nearPressureStiffness;

    workerPool_->parallelFor(state.predX.size(), [this, &state](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const float pressureSource = glm::clamp(state.densityError[i], -0.18f, 2.0f);
            state.densityPressure[i] += -pressureSource * state.alpha[i];
        }
    });

    workerPool_->parallelFor(
        state.predX.size(),
        [this, &state, targetSeparation, maxCorrection, correctionScale, tensileScale](std::size_t begin, std::size_t end) {
            for (std::size_t i = begin; i < end; ++i) {
                glm::vec3 correction(0.0f);

                forEachNeighbor(
                    i,
                    state,
                    state.predX,
                    state.predY,
                    state.predZ,
                    [&](std::size_t neighbor, float dx, float dy, float dz, float distanceSquared) {
                        const float distance = std::sqrt(std::max(distanceSquared, kEpsilon));
                        const glm::vec3 delta(dx, dy, dz);
                        const glm::vec3 gradient =
                            spikyGradient(delta, distance, settings_.smoothingLength, spikyGradientCoefficient_);
                        const float corr =
                            -tensileScale *
                            std::pow(
                                poly6Kernel(distanceSquared, smoothingLengthSquared_, poly6Coefficient_) /
                                    correctionReferenceKernel_,
                                4.0f
                            );
                        correction +=
                            (state.densityPressure[i] + state.densityPressure[neighbor] + corr) * gradient;

                        if (distance < targetSeparation) {
                            const float overlap = targetSeparation - distance;
                            correction += (delta / distance) * (overlap * 0.014f);
                        }
                    }
                );

                correction *= correctionScale;
                const float correctionLength = glm::length(correction);
                if (correctionLength > maxCorrection && correctionLength > kEpsilon) {
                    correction *= maxCorrection / correctionLength;
                }

                state.deltaX[i] = correction.x;
                state.deltaY[i] = correction.y;
                state.deltaZ[i] = correction.z;
            }
        }
    );
}

void WaterSimulation::applyPredictedCorrections() {
    auto& state = activeState_;
    workerPool_->parallelFor(state.predX.size(), [this, &state](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            float px = state.predX[i] + state.deltaX[i];
            float py = state.predY[i] + state.deltaY[i];
            float pz = state.predZ[i] + state.deltaZ[i];
            enforceBounds(px, py, pz);
            state.predX[i] = px;
            state.predY[i] = py;
            state.predZ[i] = pz;
        }
    });
}

void WaterSimulation::updateProvisionalVelocities(float deltaTime) {
    auto& state = activeState_;
    workerPool_->parallelFor(state.predX.size(), [this, &state, deltaTime](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            state.provisionalVelX[i] = (state.predX[i] - state.posX[i]) / std::max(deltaTime, kEpsilon);
            state.provisionalVelY[i] = (state.predY[i] - state.posY[i]) / std::max(deltaTime, kEpsilon);
            state.provisionalVelZ[i] = (state.predZ[i] - state.posZ[i]) / std::max(deltaTime, kEpsilon);
        }
    });
}

void WaterSimulation::computeDivergenceAndAlpha(float deltaTime, std::size_t& neighborSamples) {
    auto& state = activeState_;
    const float restDensity = std::max(settings_.restDensity, kEpsilon);
    const float alphaEpsilon = 4.0e-3f;
    const float densityGradientScale = settings_.particleMass / restDensity;
    std::atomic<std::size_t> neighborCounter{0};

    workerPool_->parallelFor(
        state.predX.size(),
        [this, &state, deltaTime, densityGradientScale, alphaEpsilon, &neighborCounter](std::size_t begin, std::size_t end) {
            std::size_t localNeighbors = 0;
            for (std::size_t i = begin; i < end; ++i) {
                const glm::vec3 velocityI(
                    state.provisionalVelX[i],
                    state.provisionalVelY[i],
                    state.provisionalVelZ[i]
                );
                glm::vec3 gradI(0.0f);
                float sumGradients = 0.0f;
                float divergence = 0.0f;

                forEachNeighbor(
                    i,
                    state,
                    state.predX,
                    state.predY,
                    state.predZ,
                    [&](std::size_t neighbor, float dx, float dy, float dz, float distanceSquared) {
                        const float distance = std::sqrt(std::max(distanceSquared, kEpsilon));
                        const glm::vec3 gradient =
                            densityGradientScale *
                            spikyGradient(
                                glm::vec3(dx, dy, dz),
                                distance,
                                settings_.smoothingLength,
                                spikyGradientCoefficient_
                            );
                        const glm::vec3 velocityJ(
                            state.provisionalVelX[neighbor],
                            state.provisionalVelY[neighbor],
                            state.provisionalVelZ[neighbor]
                        );
                        sumGradients += glm::dot(gradient, gradient);
                        gradI += gradient;
                        divergence += glm::dot(velocityI - velocityJ, gradient);
                        ++localNeighbors;
                    }
                );

                sumGradients += glm::dot(gradI, gradI);
                state.alpha[i] = 1.0f / (sumGradients + alphaEpsilon);
                state.divergenceError[i] = glm::clamp(divergence * deltaTime, 0.0f, 2.0f);
            }
            neighborCounter.fetch_add(localNeighbors, std::memory_order_relaxed);
        }
    );

    neighborSamples += neighborCounter.load(std::memory_order_relaxed);
}

void WaterSimulation::solveDivergenceIteration(float deltaTime) {
    auto& state = activeState_;

    workerPool_->parallelFor(state.predX.size(), [this, &state](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            state.divergencePressure[i] += -state.divergenceError[i] * state.alpha[i];
        }
    });

    workerPool_->parallelFor(state.predX.size(), [this, &state, deltaTime](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            glm::vec3 velocityCorrection(0.0f);

            forEachNeighbor(
                i,
                state,
                state.predX,
                state.predY,
                state.predZ,
                [&](std::size_t neighbor, float dx, float dy, float dz, float distanceSquared) {
                    const float distance = std::sqrt(std::max(distanceSquared, kEpsilon));
                    const glm::vec3 gradient =
                        spikyGradient(
                            glm::vec3(dx, dy, dz),
                            distance,
                            settings_.smoothingLength,
                            spikyGradientCoefficient_
                        );
                    velocityCorrection +=
                        (state.divergencePressure[i] + state.divergencePressure[neighbor]) *
                        settings_.particleMass *
                        gradient;
                }
            );

            state.provisionalVelX[i] += velocityCorrection.x * deltaTime;
            state.provisionalVelY[i] += velocityCorrection.y * deltaTime;
            state.provisionalVelZ[i] += velocityCorrection.z * deltaTime;

            const float speedSquared =
                state.provisionalVelX[i] * state.provisionalVelX[i] +
                state.provisionalVelY[i] * state.provisionalVelY[i] +
                state.provisionalVelZ[i] * state.provisionalVelZ[i];
            const float maxSpeedSquared = settings_.maxSpeed * settings_.maxSpeed;
            if (speedSquared > maxSpeedSquared && speedSquared > kEpsilon) {
                const float scale = settings_.maxSpeed / std::sqrt(speedSquared);
                state.provisionalVelX[i] *= scale;
                state.provisionalVelY[i] *= scale;
                state.provisionalVelZ[i] *= scale;
            }
        }
    });
}

void WaterSimulation::computeFinalState(float deltaTime, std::size_t& neighborSamples) {
    auto& state = activeState_;
    const float minX = -halfDomain_ + settings_.particleRadius;
    const float maxX = halfDomain_ - settings_.particleRadius;
    const float minY = settings_.containerFloorY + settings_.particleRadius;
    const float minZ = -halfDomain_ + settings_.particleRadius;
    const float maxZ = halfDomain_ - settings_.particleRadius;
    const float restDensity = std::max(settings_.restDensity, kEpsilon);
    const float foamDamping = std::exp(-settings_.foamDecay * deltaTime);
    const float tangentialScale =
        std::max(0.0f, 1.0f - settings_.boundaryFriction - settings_.boundaryDamping * 0.15f);
    std::atomic<std::size_t> neighborCounter{0};

    workerPool_->parallelFor(
        state.predX.size(),
        [this, &state, minX, maxX, minY, minZ, maxZ, restDensity, foamDamping, tangentialScale, deltaTime, &neighborCounter](std::size_t begin, std::size_t end) {
            std::size_t localNeighbors = 0;
            for (std::size_t i = begin; i < end; ++i) {
                const glm::vec3 predicted(state.predX[i], state.predY[i], state.predZ[i]);
                const glm::vec3 previousVelocity(state.velX[i], state.velY[i], state.velZ[i]);
                const glm::vec3 baseVelocity(
                    state.provisionalVelX[i],
                    state.provisionalVelY[i],
                    state.provisionalVelZ[i]
                );

                float density = settings_.particleMass * selfDensityKernel_;
                glm::vec3 xsph(0.0f);
                glm::vec3 pairwiseViscosity(0.0f);
                forEachNeighbor(
                    i,
                    state,
                    state.predX,
                    state.predY,
                    state.predZ,
                    [&](std::size_t neighbor, float dx, float dy, float dz, float distanceSquared) {
                        const float kernel = poly6Kernel(distanceSquared, smoothingLengthSquared_, poly6Coefficient_);
                        density += settings_.particleMass * kernel;
                        const glm::vec3 neighborVelocity(
                            state.provisionalVelX[neighbor] - state.provisionalVelX[i],
                            state.provisionalVelY[neighbor] - state.provisionalVelY[i],
                            state.provisionalVelZ[neighbor] - state.provisionalVelZ[i]
                        );
                        xsph += neighborVelocity * kernel;

                        const float distance = std::sqrt(std::max(distanceSquared, kEpsilon));
                        const float q = glm::clamp(1.0f - distance / settings_.smoothingLength, 0.0f, 1.0f);
                        if (q > 0.0f) {
                            const glm::vec3 radialDirection(dx, dy, dz);
                            const glm::vec3 normalizedDirection = radialDirection / distance;
                            const float radialVelocity = glm::dot(-neighborVelocity, normalizedDirection);
                            if (radialVelocity < 0.0f) {
                                const float approachSpeed = -radialVelocity;
                                const float viscosityImpulse =
                                    deltaTime *
                                    q *
                                    (settings_.viscosityLinear * approachSpeed +
                                     settings_.viscosityQuadratic * approachSpeed * approachSpeed);
                                pairwiseViscosity += normalizedDirection * (0.5f * viscosityImpulse);
                            }
                        }
                        ++localNeighbors;
                    }
                );

                state.density[i] = density;
                const float densityRatio = density / restDensity;
                const float densityError = std::abs(densityRatio - 1.0f);
                const float divergenceError = std::abs(state.divergenceError[i]);

                glm::vec3 relaxedVelocity =
                    glm::mix(previousVelocity, baseVelocity, settings_.velocityTransfer) +
                    pairwiseViscosity +
                    xsph * (settings_.xsphViscosity * settings_.particleMass / std::max(density, kEpsilon));

                const float projectedSpeed = glm::length(baseVelocity);
                const float quietSpeedWeight =
                    glm::clamp(1.0f - projectedSpeed / kQuietSpeedThreshold, 0.0f, 1.0f);
                const float quietDensityWeight =
                    glm::clamp(1.0f - densityError / kQuietDensityErrorThreshold, 0.0f, 1.0f);
                const float quietWeight = quietSpeedWeight * quietDensityWeight;
                relaxedVelocity *= std::exp(-settings_.restVelocityDamping * quietWeight * deltaTime);

                if (predicted.x <= minX + kEpsilon && relaxedVelocity.x < 0.0f) {
                    relaxedVelocity.x = -relaxedVelocity.x * settings_.boundaryRestitution;
                    relaxedVelocity.y *= tangentialScale;
                    relaxedVelocity.z *= tangentialScale;
                } else if (predicted.x >= maxX - kEpsilon && relaxedVelocity.x > 0.0f) {
                    relaxedVelocity.x = -relaxedVelocity.x * settings_.boundaryRestitution;
                    relaxedVelocity.y *= tangentialScale;
                    relaxedVelocity.z *= tangentialScale;
                }

                if (predicted.y <= minY + kEpsilon && relaxedVelocity.y < 0.0f) {
                    relaxedVelocity.y = -relaxedVelocity.y * settings_.boundaryRestitution;
                    relaxedVelocity.x *= tangentialScale;
                    relaxedVelocity.z *= tangentialScale;
                    if (std::abs(relaxedVelocity.y) < 0.04f) {
                        relaxedVelocity.y = 0.0f;
                    }
                }

                if (predicted.z <= minZ + kEpsilon && relaxedVelocity.z < 0.0f) {
                    relaxedVelocity.z = -relaxedVelocity.z * settings_.boundaryRestitution;
                    relaxedVelocity.x *= tangentialScale;
                    relaxedVelocity.y *= tangentialScale;
                } else if (predicted.z >= maxZ - kEpsilon && relaxedVelocity.z > 0.0f) {
                    relaxedVelocity.z = -relaxedVelocity.z * settings_.boundaryRestitution;
                    relaxedVelocity.x *= tangentialScale;
                    relaxedVelocity.y *= tangentialScale;
                }

                relaxedVelocity *= std::exp(-settings_.boundaryDamping * quietWeight * deltaTime * 0.65f);

                const float speed = glm::length(relaxedVelocity);
                if (speed > settings_.maxSpeed && speed > kEpsilon) {
                    relaxedVelocity *= settings_.maxSpeed / speed;
                }

                state.posX[i] = state.predX[i];
                state.posY[i] = state.predY[i];
                state.posZ[i] = state.predZ[i];
                state.velX[i] = relaxedVelocity.x;
                state.velY[i] = relaxedVelocity.y;
                state.velZ[i] = relaxedVelocity.z;

                state.densityMetric[i] = glm::clamp(densityRatio, 0.0f, 2.0f);
                state.speedMetric[i] = glm::clamp(speed / std::max(settings_.maxSpeed, kEpsilon), 0.0f, 1.0f);
                state.pressureMetric[i] =
                    glm::clamp(densityError * 1.2f + divergenceError * 0.8f + quietWeight * 0.05f, 0.0f, 1.0f);

                const float churn =
                    glm::clamp((densityRatio - 0.95f) * 0.45f + state.speedMetric[i] * 0.22f, 0.0f, 1.0f);
                state.foam[i] = std::max(state.foam[i] * foamDamping, churn);
                state.interactionHeat[i] *= std::exp(-settings_.foamDecay * deltaTime);
                state.interactionMetric[i] =
                    glm::clamp(std::max(state.interactionHeat[i], state.foam[i] * 0.45f), 0.0f, 1.0f);
            }
            neighborCounter.fetch_add(localNeighbors, std::memory_order_relaxed);
        }
    );

    neighborSamples += neighborCounter.load(std::memory_order_relaxed);

    float densitySum = 0.0f;
    float densityErrorSum = 0.0f;
    float divergenceErrorSum = 0.0f;
    float maxObservedSpeed = 0.0f;
    for (std::size_t i = 0; i < state.posX.size(); ++i) {
        densitySum += state.density[i];
        densityErrorSum += std::abs(state.density[i] - restDensity) / restDensity;
        divergenceErrorSum += std::abs(state.divergenceError[i]);
        maxObservedSpeed = std::max(
            maxObservedSpeed,
            std::sqrt(state.velX[i] * state.velX[i] + state.velY[i] * state.velY[i] + state.velZ[i] * state.velZ[i])
        );
    }

    const float particleCountFloat = static_cast<float>(std::max<std::size_t>(state.posX.size(), 1));
    stats_.averageDensity = densitySum / particleCountFloat;
    stats_.averageDensityError = densityErrorSum / particleCountFloat;
    stats_.averageDivergenceError = divergenceErrorSum / particleCountFloat;
    stats_.maxSpeed = maxObservedSpeed;
}

void WaterSimulation::step(float deltaTime) {
    sanitizeSettings();

    if (settings_.backend == SimulationBackend::Metal && metalBackend_ != nullptr && metalBackend_->available()) {
        if (activeBackend_ != SimulationBackend::Metal) {
            reset();
        } else {
            metalBackend_->updateSettings(settings_, false);
        }

        if (activeBackend_ == SimulationBackend::Metal) {
            metalBackend_->step(deltaTime);
            stats_ = metalBackend_->stats();
            interactionPlaneY_ = metalBackend_->interactionPlaneY();
            return;
        }
    } else if (activeBackend_ == SimulationBackend::Metal) {
        reset();
    }

    const auto totalStart = Clock::now();
    stats_.frameIndex += 1;
    stats_.particleCount = particleCount();
    stats_.activeCells = 0;
    stats_.neighborSamples = 0;
    stats_.gridBuildMs = 0.0f;
    stats_.densityPassMs = 0.0f;
    stats_.constraintPassMs = 0.0f;
    stats_.divergencePassMs = 0.0f;
    stats_.integrateMs = 0.0f;
    stats_.finalizeMs = 0.0f;
    stats_.snapshotMs = 0.0f;
    stats_.simulatedDeltaTime = glm::clamp(deltaTime, kMinDeltaTime, kMaxDeltaTime);
    stats_.executedDivergenceIterations = 0;
    stats_.snapshotInterval = 1;
    stats_.averageDivergenceError = 0.0f;
    stats_.realTimeRatio = 1.0f;

    const float clampedDeltaTime = stats_.simulatedDeltaTime;
    const float safeTravel = std::max(settings_.cflFactor * settings_.smoothingLength, settings_.particleRadius * 0.5f);
    const float travelEstimate =
        stats_.maxSpeed * clampedDeltaTime + settings_.gravity * clampedDeltaTime * clampedDeltaTime;
    const int cflSubsteps = std::max(1, static_cast<int>(std::ceil(travelEstimate / std::max(safeTravel, kEpsilon))));
    const int effectiveSubsteps = std::clamp(
        std::max(settings_.substeps, cflSubsteps),
        1,
        std::max(settings_.maxSubsteps, settings_.substeps)
    );
    stats_.executedSubsteps = effectiveSubsteps;
    stats_.executedSolverIterations = settings_.pressureIterations;
    stats_.executedDivergenceIterations = std::max(1, settings_.pressureIterations / 2);
    const float substepDelta = clampedDeltaTime / static_cast<float>(effectiveSubsteps);

    for (int substep = 0; substep < effectiveSubsteps; ++substep) {
        auto& state = activeState_;
        auto phaseStart = Clock::now();
        integratePredictedPositions(substepDelta);
        stats_.integrateMs += toMilliseconds(Clock::now() - phaseStart);

        std::fill(state.densityPressure.begin(), state.densityPressure.end(), 0.0f);
        std::fill(state.divergencePressure.begin(), state.divergencePressure.end(), 0.0f);
        std::fill(state.divergenceError.begin(), state.divergenceError.end(), 0.0f);

        std::size_t neighborSamples = 0;
        for (int iteration = 0; iteration < settings_.pressureIterations; ++iteration) {
            phaseStart = Clock::now();
            rebuildSpatialGrid(state.predX, state.predY, state.predZ);
            stats_.gridBuildMs += toMilliseconds(Clock::now() - phaseStart);

            phaseStart = Clock::now();
            computeDensityAndAlpha(neighborSamples);
            stats_.densityPassMs += toMilliseconds(Clock::now() - phaseStart);

            phaseStart = Clock::now();
            solveDensityPressureIteration();
            applyPredictedCorrections();
            stats_.constraintPassMs += toMilliseconds(Clock::now() - phaseStart);
        }
        stats_.neighborSamples += neighborSamples;

        phaseStart = Clock::now();
        rebuildSpatialGrid(state.predX, state.predY, state.predZ);
        stats_.gridBuildMs += toMilliseconds(Clock::now() - phaseStart);

        std::fill(state.divergencePressure.begin(), state.divergencePressure.end(), 0.0f);

        phaseStart = Clock::now();
        updateProvisionalVelocities(substepDelta);
        stats_.divergencePassMs += toMilliseconds(Clock::now() - phaseStart);

        neighborSamples = 0;
        phaseStart = Clock::now();
        computeDivergenceAndAlpha(substepDelta, neighborSamples);
        stats_.divergencePassMs += toMilliseconds(Clock::now() - phaseStart);
        for (int iteration = 0; iteration < stats_.executedDivergenceIterations; ++iteration) {
            if (iteration > 0) {
                phaseStart = Clock::now();
                computeDivergenceAndAlpha(substepDelta, neighborSamples);
                stats_.divergencePassMs += toMilliseconds(Clock::now() - phaseStart);
            }

            phaseStart = Clock::now();
            solveDivergenceIteration(substepDelta);
            stats_.divergencePassMs += toMilliseconds(Clock::now() - phaseStart);
        }
        stats_.neighborSamples += neighborSamples;

        neighborSamples = 0;
        phaseStart = Clock::now();
        computeFinalState(substepDelta, neighborSamples);
        stats_.finalizeMs += toMilliseconds(Clock::now() - phaseStart);
        stats_.neighborSamples += neighborSamples;
    }

    updateDerivedState();
    stats_.totalStepMs = toMilliseconds(Clock::now() - totalStart);
}

void WaterSimulation::addImpulse(
    const glm::vec2& worldXZ,
    const glm::vec2& dragVelocity,
    float heightDelta,
    float radius
) {
    if (settings_.backend == SimulationBackend::Metal && metalBackend_ != nullptr && metalBackend_->available()) {
        if (activeBackend_ != SimulationBackend::Metal) {
            reset();
        } else {
            metalBackend_->updateSettings(settings_, false);
        }
        if (activeBackend_ == SimulationBackend::Metal) {
            metalBackend_->addImpulse(worldXZ, dragVelocity, heightDelta, radius);
            stats_ = metalBackend_->stats();
            interactionPlaneY_ = metalBackend_->interactionPlaneY();
            return;
        }
    }

    if (std::abs(worldXZ.x) > halfDomain_ || std::abs(worldXZ.y) > halfDomain_) {
        return;
    }

    glm::vec2 clampedDrag = dragVelocity;
    const float dragMagnitude = glm::length(clampedDrag);
    if (dragMagnitude > settings_.interactionMaxSpeed && dragMagnitude > kEpsilon) {
        clampedDrag *= settings_.interactionMaxSpeed / dragMagnitude;
    }

    const float clampedRadius = std::max(radius, settings_.particleRadius * 2.5f);
    const float radiusSquared = clampedRadius * clampedRadius;
    const float verticalImpulse =
        glm::clamp(-heightDelta * settings_.interactionLift * 52.0f, -1.15f, 1.15f);
    const glm::vec3 directionalImpulse(
        clampedDrag.x * settings_.interactionForce,
        verticalImpulse,
        clampedDrag.y * settings_.interactionForce
    );

    const glm::vec3 center(worldXZ.x, interactionPlaneY_, worldXZ.y);
    const glm::vec3 minPoint = center - glm::vec3(clampedRadius);
    const glm::vec3 maxPoint = center + glm::vec3(clampedRadius);
    const glm::ivec3 minCell = positionToCell(minPoint.x, minPoint.y, minPoint.z);
    const glm::ivec3 maxCell = positionToCell(maxPoint.x, maxPoint.y, maxPoint.z);
    auto& state = activeState_;

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                const int cellIndex = (z * gridResolutionY_ + y) * gridResolutionX_ + x;
                for (int entry = cellStarts_[static_cast<std::size_t>(cellIndex)];
                     entry < cellStarts_[static_cast<std::size_t>(cellIndex) + 1];
                     ++entry) {
                    const std::size_t particleIndex = static_cast<std::size_t>(entry);
                    const glm::vec3 offset(
                        state.posX[particleIndex] - center.x,
                        state.posY[particleIndex] - center.y,
                        state.posZ[particleIndex] - center.z
                    );
                    const float distanceSquared = glm::dot(offset, offset);
                    if (distanceSquared > radiusSquared) {
                        continue;
                    }

                    const float distance = std::sqrt(std::max(distanceSquared, 0.0f));
                    const float q = 1.0f - distance / clampedRadius;
                    const float influence = smoothPulse(q);
                    const glm::vec3 swirl =
                        glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), offset) * (0.055f * influence);

                    state.velX[particleIndex] += (directionalImpulse.x + swirl.x) * influence;
                    state.velY[particleIndex] += (directionalImpulse.y + swirl.y) * influence;
                    state.velZ[particleIndex] += (directionalImpulse.z + swirl.z) * influence;

                    const float speed = std::sqrt(
                        state.velX[particleIndex] * state.velX[particleIndex] +
                        state.velY[particleIndex] * state.velY[particleIndex] +
                        state.velZ[particleIndex] * state.velZ[particleIndex]
                    );
                    if (speed > settings_.maxSpeed && speed > kEpsilon) {
                        const float scale = settings_.maxSpeed / speed;
                        state.velX[particleIndex] *= scale;
                        state.velY[particleIndex] *= scale;
                        state.velZ[particleIndex] *= scale;
                    }

                    state.interactionHeat[particleIndex] =
                        std::max(state.interactionHeat[particleIndex], influence * 0.85f);
                    state.foam[particleIndex] = std::max(state.foam[particleIndex], influence * 0.55f);
                }
            }
        }
    }
}

void WaterSimulation::buildRenderSnapshot(WaterRenderSnapshot& snapshot) const {
    if (activeBackend_ == SimulationBackend::Metal && metalBackend_ != nullptr) {
        metalBackend_->buildRenderSnapshot(snapshot);
        return;
    }

    const auto& state = activeState_;
    snapshot.particles.resize(state.posX.size());
    for (std::size_t i = 0; i < state.posX.size(); ++i) {
        const std::size_t stableIndex = static_cast<std::size_t>(state.stableParticleIds[i]);
        snapshot.particles[stableIndex].position = glm::vec3(state.posX[i], state.posY[i], state.posZ[i]);
        snapshot.particles[stableIndex].metrics = glm::vec4(
            state.densityMetric[i],
            state.speedMetric[i],
            state.pressureMetric[i],
            state.interactionMetric[i]
        );
    }

    snapshot.particleRadius = settings_.particleRadius;
    snapshot.interactionPlaneY = interactionPlaneY_;
    snapshot.halfDomainSize = halfDomain_;
    snapshot.containerFloorY = settings_.containerFloorY;
    snapshot.containerLipY = settings_.containerLipY;
    snapshot.version = stats_.frameIndex;
}

WaterSimulationSettings& WaterSimulation::settings() {
    return settings_;
}

const WaterSimulationSettings& WaterSimulation::settings() const {
    return settings_;
}

const SimulationStats& WaterSimulation::stats() const {
    if (activeBackend_ == SimulationBackend::Metal && metalBackend_ != nullptr) {
        return metalBackend_->stats();
    }
    return stats_;
}

float WaterSimulation::domainSize() const {
    return settings_.domainSize;
}

float WaterSimulation::halfDomainSize() const {
    return halfDomain_;
}

float WaterSimulation::cellSize() const {
    return cellSize_;
}

float WaterSimulation::containerFloorY() const {
    return settings_.containerFloorY;
}

float WaterSimulation::containerLipY() const {
    return settings_.containerLipY;
}

float WaterSimulation::interactionPlaneY() const {
    if (activeBackend_ == SimulationBackend::Metal && metalBackend_ != nullptr) {
        return metalBackend_->interactionPlaneY();
    }
    return interactionPlaneY_;
}

float WaterSimulation::particleRadius() const {
    return settings_.particleRadius;
}

float WaterSimulation::restDensity() const {
    return settings_.restDensity;
}

std::size_t WaterSimulation::particleCount() const {
    if (activeBackend_ == SimulationBackend::Metal && metalBackend_ != nullptr) {
        return metalBackend_->particleCount();
    }
    return activeState_.posX.size();
}

void WaterSimulation::enforceBounds(float& x, float& y, float& z) const {
    x = glm::clamp(x, -halfDomain_ + settings_.particleRadius, halfDomain_ - settings_.particleRadius);
    y = std::max(y, settings_.containerFloorY + settings_.particleRadius);
    z = glm::clamp(z, -halfDomain_ + settings_.particleRadius, halfDomain_ - settings_.particleRadius);
}

void WaterSimulation::updateDerivedState() {
    const auto& state = activeState_;
    if (state.posY.empty()) {
        interactionPlaneY_ = settings_.containerFloorY + settings_.particleRadius * 3.0f;
        return;
    }

    float averageY = 0.0f;
    float maxY = state.posY.front();
    for (float y : state.posY) {
        averageY += y;
        maxY = std::max(maxY, y);
    }
    averageY /= static_cast<float>(state.posY.size());

    interactionPlaneY_ = glm::clamp(
        glm::mix(averageY + settings_.smoothingLength * 0.8f, maxY - settings_.particleRadius * 0.4f, 0.28f),
        settings_.containerFloorY + settings_.particleRadius * 2.3f,
        settings_.containerLipY - settings_.particleRadius * 2.1f
    );
}

glm::ivec3 WaterSimulation::positionToCell(float x, float y, float z) const {
    const glm::vec3 normalized = (glm::vec3(x, y, z) - gridMin_) / cellSize_;
    return glm::ivec3(
        glm::clamp(static_cast<int>(std::floor(normalized.x)), 0, gridResolutionX_ - 1),
        glm::clamp(static_cast<int>(std::floor(normalized.y)), 0, gridResolutionY_ - 1),
        glm::clamp(static_cast<int>(std::floor(normalized.z)), 0, gridResolutionZ_ - 1)
    );
}

int WaterSimulation::positionToCellIndex(float x, float y, float z) const {
    const glm::ivec3 cell = positionToCell(x, y, z);
    return (cell.z * gridResolutionY_ + cell.y) * gridResolutionX_ + cell.x;
}
