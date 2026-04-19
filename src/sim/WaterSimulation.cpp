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

float poly6Kernel(float distanceSquared, float smoothingLength) {
    if (distanceSquared >= smoothingLength * smoothingLength) {
        return 0.0f;
    }

    const float h2MinusR2 = smoothingLength * smoothingLength - distanceSquared;
    const float coefficient = 315.0f / (64.0f * kPi * std::pow(smoothingLength, 9.0f));
    return coefficient * h2MinusR2 * h2MinusR2 * h2MinusR2;
}

glm::vec3 spikyGradient(const glm::vec3& delta, float distance, float smoothingLength) {
    if (distance <= kEpsilon || distance >= smoothingLength) {
        return glm::vec3(0.0f);
    }

    const float coefficient = -45.0f / (kPi * std::pow(smoothingLength, 6.0f));
    const float scale = coefficient * (smoothingLength - distance) * (smoothingLength - distance) / distance;
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
    gridMin_ = glm::vec3(-halfDomain_, settings_.containerFloorY, -halfDomain_);
    gridMax_ = glm::vec3(halfDomain_, settings_.containerLipY, halfDomain_);

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

    posX_.assign(particleCount, 0.0f);
    posY_.assign(particleCount, 0.0f);
    posZ_.assign(particleCount, 0.0f);
    predX_.assign(particleCount, 0.0f);
    predY_.assign(particleCount, 0.0f);
    predZ_.assign(particleCount, 0.0f);
    velX_.assign(particleCount, 0.0f);
    velY_.assign(particleCount, 0.0f);
    velZ_.assign(particleCount, 0.0f);
    density_.assign(particleCount, settings_.restDensity);
    lambda_.assign(particleCount, 0.0f);
    deltaX_.assign(particleCount, 0.0f);
    deltaY_.assign(particleCount, 0.0f);
    deltaZ_.assign(particleCount, 0.0f);
    speedMetric_.assign(particleCount, 0.0f);
    densityMetric_.assign(particleCount, 1.0f);
    pressureMetric_.assign(particleCount, 0.0f);
    interactionMetric_.assign(particleCount, 0.0f);
    foam_.assign(particleCount, 0.0f);
    interactionHeat_.assign(particleCount, 0.0f);
    particleCellIndices_.assign(particleCount, 0);
    cellCounts_.assign(cellCount, 0);
    cellStarts_.assign(cellCount + 1, 0);
    sortedParticleIndices_.assign(particleCount, 0);
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

                posX_[index] = px;
                posY_[index] = py;
                posZ_[index] = pz;
                predX_[index] = px;
                predY_[index] = py;
                predZ_[index] = pz;
                ++index;
            }
        }
    }

    spawnTopY_ = startY + static_cast<float>(settings_.particlesY - 1) * spacing + settings_.particleRadius;
    rebuildSpatialGrid(posX_, posY_, posZ_);
    std::size_t neighborSamples = 0;
    computeFinalState(1.0f / 120.0f, neighborSamples);
    updateDerivedState();
    stats_.frameIndex = 0;
}

void WaterSimulation::rebuildSpatialGrid(
    const std::vector<float>& xs,
    const std::vector<float>& ys,
    const std::vector<float>& zs
) {
    std::fill(cellCounts_.begin(), cellCounts_.end(), 0);

    for (std::size_t i = 0; i < xs.size(); ++i) {
        const int cellIndex = positionToCellIndex(xs[i], ys[i], zs[i]);
        particleCellIndices_[i] = cellIndex;
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

    std::vector<int> writeHeads(cellStarts_.begin(), cellStarts_.end() - 1);
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const int cellIndex = particleCellIndices_[i];
        sortedParticleIndices_[static_cast<std::size_t>(writeHeads[cellIndex]++)] = static_cast<int>(i);
    }
}

void WaterSimulation::integratePredictedPositions(float deltaTime) {
    const float damping = std::exp(-settings_.velocityDamping * deltaTime);
    workerPool_->parallelFor(posX_.size(), [this, deltaTime, damping](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            velY_[i] -= settings_.gravity * deltaTime;
            velX_[i] *= damping;
            velY_[i] *= damping;
            velZ_[i] *= damping;

            const float speedSquared = velX_[i] * velX_[i] + velY_[i] * velY_[i] + velZ_[i] * velZ_[i];
            const float maxSpeedSquared = settings_.maxSpeed * settings_.maxSpeed;
            if (speedSquared > maxSpeedSquared) {
                const float scale = settings_.maxSpeed / std::sqrt(std::max(speedSquared, kEpsilon));
                velX_[i] *= scale;
                velY_[i] *= scale;
                velZ_[i] *= scale;
            }

            float px = posX_[i] + velX_[i] * deltaTime;
            float py = posY_[i] + velY_[i] * deltaTime;
            float pz = posZ_[i] + velZ_[i] * deltaTime;
            enforceBounds(px, py, pz);
            predX_[i] = px;
            predY_[i] = py;
            predZ_[i] = pz;
        }
    });
}

void WaterSimulation::computeDensityAndLambdas(std::size_t& neighborSamples) {
    const float w0 = poly6Kernel(0.0f, settings_.smoothingLength);
    const float restDensity = std::max(settings_.restDensity, kEpsilon);
    const float lambdaEpsilon = 4.0e-3f;
    std::atomic<std::size_t> neighborCounter{0};

    workerPool_->parallelFor(
        predX_.size(),
        [this, w0, restDensity, lambdaEpsilon, &neighborCounter](std::size_t begin, std::size_t end) {
            std::size_t localNeighbors = 0;
            for (std::size_t i = begin; i < end; ++i) {
                float density = settings_.particleMass * w0;
                forEachNeighbor(i, predX_, predY_, predZ_, [&](std::size_t, float, float, float, float distanceSquared) {
                    density += settings_.particleMass * poly6Kernel(distanceSquared, settings_.smoothingLength);
                    ++localNeighbors;
                });
                density_[i] = density;

                const float constraint = glm::clamp(density / restDensity - 1.0f, -0.12f, 1.5f);
                glm::vec3 gradI(0.0f);
                float sumGradients = 0.0f;

                forEachNeighbor(i, predX_, predY_, predZ_, [&](std::size_t, float dx, float dy, float dz, float distanceSquared) {
                    const float distance = std::sqrt(std::max(distanceSquared, kEpsilon));
                    const glm::vec3 gradient =
                        (settings_.particleMass / restDensity) *
                        spikyGradient(glm::vec3(dx, dy, dz), distance, settings_.smoothingLength);
                    sumGradients += glm::dot(gradient, gradient);
                    gradI += gradient;
                });
                sumGradients += glm::dot(gradI, gradI);

                lambda_[i] = -constraint / (sumGradients + lambdaEpsilon);
            }
            neighborCounter.fetch_add(localNeighbors, std::memory_order_relaxed);
        }
    );

    neighborSamples += neighborCounter.load(std::memory_order_relaxed);
}

void WaterSimulation::computePositionCorrections(float) {
    const float correctionReference = std::max(
        poly6Kernel(settings_.smoothingLength * settings_.smoothingLength * 0.12f, settings_.smoothingLength),
        kEpsilon
    );
    const float targetSeparation = settings_.particleRadius * 1.65f;
    const float maxCorrection = settings_.particleRadius * 0.22f;
    const float correctionScale = settings_.positionRelaxation * settings_.pressureStiffness / std::max(settings_.restDensity, kEpsilon);
    const float tensileScale = settings_.tensileCorrection * settings_.nearPressureStiffness;

    workerPool_->parallelFor(
        predX_.size(),
        [this, correctionReference, targetSeparation, maxCorrection, correctionScale, tensileScale](std::size_t begin, std::size_t end) {
            for (std::size_t i = begin; i < end; ++i) {
                glm::vec3 correction(0.0f);

                forEachNeighbor(i, predX_, predY_, predZ_, [&](std::size_t neighbor, float dx, float dy, float dz, float distanceSquared) {
                    const float distance = std::sqrt(std::max(distanceSquared, kEpsilon));
                    const glm::vec3 delta(dx, dy, dz);
                    const glm::vec3 gradient = spikyGradient(delta, distance, settings_.smoothingLength);
                    const float corr =
                        -tensileScale *
                        std::pow(poly6Kernel(distanceSquared, settings_.smoothingLength) / correctionReference, 4.0f);
                    correction += (lambda_[i] + lambda_[neighbor] + corr) * gradient;

                    if (distance < targetSeparation) {
                        const float overlap = targetSeparation - distance;
                        correction += (delta / distance) * (overlap * 0.025f);
                    }
                });

                correction *= correctionScale;
                const float correctionLength = glm::length(correction);
                if (correctionLength > maxCorrection && correctionLength > kEpsilon) {
                    correction *= maxCorrection / correctionLength;
                }

                deltaX_[i] = correction.x;
                deltaY_[i] = correction.y;
                deltaZ_[i] = correction.z;
            }
        }
    );
}

void WaterSimulation::applyPredictedCorrections() {
    workerPool_->parallelFor(predX_.size(), [this](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            float px = predX_[i] + deltaX_[i];
            float py = predY_[i] + deltaY_[i];
            float pz = predZ_[i] + deltaZ_[i];
            enforceBounds(px, py, pz);
            predX_[i] = px;
            predY_[i] = py;
            predZ_[i] = pz;
        }
    });
}

void WaterSimulation::computeFinalState(float deltaTime, std::size_t& neighborSamples) {
    const float w0 = poly6Kernel(0.0f, settings_.smoothingLength);
    const float minX = -halfDomain_ + settings_.particleRadius;
    const float maxX = halfDomain_ - settings_.particleRadius;
    const float minY = settings_.containerFloorY + settings_.particleRadius;
    const float maxY = settings_.containerLipY - settings_.particleRadius;
    const float minZ = -halfDomain_ + settings_.particleRadius;
    const float maxZ = halfDomain_ - settings_.particleRadius;
    const float restDensity = std::max(settings_.restDensity, kEpsilon);
    const float foamDamping = std::exp(-settings_.foamDecay * deltaTime);
    const float tangentialScale =
        std::max(0.0f, 1.0f - settings_.boundaryFriction - settings_.boundaryDamping * 0.15f);
    std::atomic<std::size_t> neighborCounter{0};

    workerPool_->parallelFor(
        predX_.size(),
        [this, w0, minX, maxX, minY, maxY, minZ, maxZ, restDensity, foamDamping, tangentialScale, deltaTime, &neighborCounter](std::size_t begin, std::size_t end) {
            std::size_t localNeighbors = 0;
            for (std::size_t i = begin; i < end; ++i) {
                const glm::vec3 predicted(predX_[i], predY_[i], predZ_[i]);
                const glm::vec3 previousVelocity(velX_[i], velY_[i], velZ_[i]);

                float density = settings_.particleMass * w0;
                glm::vec3 xsph(0.0f);
                forEachNeighbor(i, predX_, predY_, predZ_, [&](std::size_t neighbor, float, float, float, float distanceSquared) {
                    const float kernel = poly6Kernel(distanceSquared, settings_.smoothingLength);
                    density += settings_.particleMass * kernel;
                    xsph += glm::vec3(
                        velX_[neighbor] - velX_[i],
                        velY_[neighbor] - velY_[i],
                        velZ_[neighbor] - velZ_[i]
                    ) * kernel;
                    ++localNeighbors;
                });

                density_[i] = density;
                const float densityRatio = density / restDensity;
                const float densityError = std::abs(densityRatio - 1.0f);

                const glm::vec3 projectedVelocity(
                    (predX_[i] - posX_[i]) / std::max(deltaTime, kEpsilon),
                    (predY_[i] - posY_[i]) / std::max(deltaTime, kEpsilon),
                    (predZ_[i] - posZ_[i]) / std::max(deltaTime, kEpsilon)
                );
                glm::vec3 relaxedVelocity =
                    previousVelocity +
                    xsph * (settings_.xsphViscosity * settings_.particleMass / std::max(density, kEpsilon));
                glm::vec3 newVelocity =
                    glm::mix(relaxedVelocity, projectedVelocity, settings_.velocityTransfer);

                const float projectedSpeed = glm::length(projectedVelocity);
                const float quietSpeedWeight =
                    glm::clamp(1.0f - projectedSpeed / kQuietSpeedThreshold, 0.0f, 1.0f);
                const float quietDensityWeight =
                    glm::clamp(1.0f - densityError / kQuietDensityErrorThreshold, 0.0f, 1.0f);
                const float quietWeight = quietSpeedWeight * quietDensityWeight;
                newVelocity *= std::exp(-settings_.restVelocityDamping * quietWeight * deltaTime);

                if (predicted.x <= minX + kEpsilon && newVelocity.x < 0.0f) {
                    newVelocity.x = -newVelocity.x * settings_.boundaryRestitution;
                    newVelocity.y *= tangentialScale;
                    newVelocity.z *= tangentialScale;
                } else if (predicted.x >= maxX - kEpsilon && newVelocity.x > 0.0f) {
                    newVelocity.x = -newVelocity.x * settings_.boundaryRestitution;
                    newVelocity.y *= tangentialScale;
                    newVelocity.z *= tangentialScale;
                }

                if (predicted.y <= minY + kEpsilon && newVelocity.y < 0.0f) {
                    newVelocity.y = -newVelocity.y * settings_.boundaryRestitution;
                    newVelocity.x *= tangentialScale;
                    newVelocity.z *= tangentialScale;
                    if (std::abs(newVelocity.y) < 0.04f) {
                        newVelocity.y = 0.0f;
                    }
                } else if (predicted.y >= maxY - kEpsilon && newVelocity.y > 0.0f) {
                    newVelocity.y = -newVelocity.y * settings_.boundaryRestitution;
                    newVelocity.x *= tangentialScale;
                    newVelocity.z *= tangentialScale;
                }

                if (predicted.z <= minZ + kEpsilon && newVelocity.z < 0.0f) {
                    newVelocity.z = -newVelocity.z * settings_.boundaryRestitution;
                    newVelocity.x *= tangentialScale;
                    newVelocity.y *= tangentialScale;
                } else if (predicted.z >= maxZ - kEpsilon && newVelocity.z > 0.0f) {
                    newVelocity.z = -newVelocity.z * settings_.boundaryRestitution;
                    newVelocity.x *= tangentialScale;
                    newVelocity.y *= tangentialScale;
                }

                newVelocity *= std::exp(-settings_.boundaryDamping * quietWeight * deltaTime * 0.65f);

                const float speed = glm::length(newVelocity);
                if (speed > settings_.maxSpeed && speed > kEpsilon) {
                    newVelocity *= settings_.maxSpeed / speed;
                }

                posX_[i] = predX_[i];
                posY_[i] = predY_[i];
                posZ_[i] = predZ_[i];
                velX_[i] = newVelocity.x;
                velY_[i] = newVelocity.y;
                velZ_[i] = newVelocity.z;

                densityMetric_[i] = glm::clamp(densityRatio, 0.0f, 2.0f);
                speedMetric_[i] = glm::clamp(speed / std::max(settings_.maxSpeed, kEpsilon), 0.0f, 1.0f);
                pressureMetric_[i] = glm::clamp(densityError * 1.5f + quietWeight * 0.05f, 0.0f, 1.0f);

                const float churn =
                    glm::clamp((densityRatio - 0.95f) * 0.45f + speedMetric_[i] * 0.22f, 0.0f, 1.0f);
                foam_[i] = std::max(foam_[i] * foamDamping, churn);
                interactionHeat_[i] *= std::exp(-settings_.foamDecay * deltaTime);
                interactionMetric_[i] = glm::clamp(std::max(interactionHeat_[i], foam_[i] * 0.45f), 0.0f, 1.0f);
            }
            neighborCounter.fetch_add(localNeighbors, std::memory_order_relaxed);
        }
    );

    neighborSamples += neighborCounter.load(std::memory_order_relaxed);

    float densitySum = 0.0f;
    float densityErrorSum = 0.0f;
    float maxObservedSpeed = 0.0f;
    for (std::size_t i = 0; i < posX_.size(); ++i) {
        densitySum += density_[i];
        densityErrorSum += std::abs(density_[i] - restDensity) / restDensity;
        maxObservedSpeed = std::max(
            maxObservedSpeed,
            std::sqrt(velX_[i] * velX_[i] + velY_[i] * velY_[i] + velZ_[i] * velZ_[i])
        );
    }

    const float particleCountFloat = static_cast<float>(std::max<std::size_t>(posX_.size(), 1));
    stats_.averageDensity = densitySum / particleCountFloat;
    stats_.averageDensityError = densityErrorSum / particleCountFloat;
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
    stats_.integrateMs = 0.0f;
    stats_.finalizeMs = 0.0f;
    stats_.snapshotMs = 0.0f;
    stats_.simulatedDeltaTime = glm::clamp(deltaTime, kMinDeltaTime, kMaxDeltaTime);

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
    const float substepDelta = clampedDeltaTime / static_cast<float>(effectiveSubsteps);

    for (int substep = 0; substep < effectiveSubsteps; ++substep) {
        auto phaseStart = Clock::now();
        integratePredictedPositions(substepDelta);
        stats_.integrateMs += toMilliseconds(Clock::now() - phaseStart);

        for (int iteration = 0; iteration < settings_.pressureIterations; ++iteration) {
            phaseStart = Clock::now();
            rebuildSpatialGrid(predX_, predY_, predZ_);
            stats_.gridBuildMs += toMilliseconds(Clock::now() - phaseStart);

            std::size_t neighborSamples = 0;
            phaseStart = Clock::now();
            computeDensityAndLambdas(neighborSamples);
            stats_.densityPassMs += toMilliseconds(Clock::now() - phaseStart);

            phaseStart = Clock::now();
            computePositionCorrections(substepDelta);
            applyPredictedCorrections();
            stats_.constraintPassMs += toMilliseconds(Clock::now() - phaseStart);
            stats_.neighborSamples += neighborSamples;
        }

        phaseStart = Clock::now();
        rebuildSpatialGrid(predX_, predY_, predZ_);
        stats_.gridBuildMs += toMilliseconds(Clock::now() - phaseStart);

        std::size_t neighborSamples = 0;
        phaseStart = Clock::now();
        computeFinalState(substepDelta, neighborSamples);
        stats_.finalizeMs += toMilliseconds(Clock::now() - phaseStart);
        stats_.neighborSamples += neighborSamples;
    }

    rebuildSpatialGrid(posX_, posY_, posZ_);
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

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                const int cellIndex = (z * gridResolutionY_ + y) * gridResolutionX_ + x;
                for (int entry = cellStarts_[static_cast<std::size_t>(cellIndex)];
                     entry < cellStarts_[static_cast<std::size_t>(cellIndex) + 1];
                     ++entry) {
                    const std::size_t particleIndex =
                        static_cast<std::size_t>(sortedParticleIndices_[static_cast<std::size_t>(entry)]);
                    const glm::vec3 offset(
                        posX_[particleIndex] - center.x,
                        posY_[particleIndex] - center.y,
                        posZ_[particleIndex] - center.z
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

                    velX_[particleIndex] += (directionalImpulse.x + swirl.x) * influence;
                    velY_[particleIndex] += (directionalImpulse.y + swirl.y) * influence;
                    velZ_[particleIndex] += (directionalImpulse.z + swirl.z) * influence;

                    const float speed = std::sqrt(
                        velX_[particleIndex] * velX_[particleIndex] +
                        velY_[particleIndex] * velY_[particleIndex] +
                        velZ_[particleIndex] * velZ_[particleIndex]
                    );
                    if (speed > settings_.maxSpeed && speed > kEpsilon) {
                        const float scale = settings_.maxSpeed / speed;
                        velX_[particleIndex] *= scale;
                        velY_[particleIndex] *= scale;
                        velZ_[particleIndex] *= scale;
                    }

                    interactionHeat_[particleIndex] = std::max(interactionHeat_[particleIndex], influence * 0.85f);
                    foam_[particleIndex] = std::max(foam_[particleIndex], influence * 0.55f);
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

    snapshot.particles.resize(posX_.size());
    for (std::size_t i = 0; i < posX_.size(); ++i) {
        snapshot.particles[i].position = glm::vec3(posX_[i], posY_[i], posZ_[i]);
        snapshot.particles[i].metrics = glm::vec4(
            densityMetric_[i],
            speedMetric_[i],
            pressureMetric_[i],
            interactionMetric_[i]
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
    return posX_.size();
}

void WaterSimulation::enforceBounds(float& x, float& y, float& z) const {
    x = glm::clamp(x, -halfDomain_ + settings_.particleRadius, halfDomain_ - settings_.particleRadius);
    y = glm::clamp(y, settings_.containerFloorY + settings_.particleRadius, settings_.containerLipY - settings_.particleRadius);
    z = glm::clamp(z, -halfDomain_ + settings_.particleRadius, halfDomain_ - settings_.particleRadius);
}

void WaterSimulation::updateDerivedState() {
    if (posY_.empty()) {
        interactionPlaneY_ = settings_.containerFloorY + settings_.particleRadius * 3.0f;
        return;
    }

    float averageY = 0.0f;
    float maxY = posY_.front();
    for (float y : posY_) {
        averageY += y;
        maxY = std::max(maxY, y);
    }
    averageY /= static_cast<float>(posY_.size());

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
