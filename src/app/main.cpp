#define GLFW_INCLUDE_NONE

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>

#include "render/WaterRenderer.h"
#include "sim/WaterSimulation.h"
#include "app/controls.h"

#ifndef RESOURCES_PATH
#define RESOURCES_PATH "../resources/"
#endif

namespace {
using Clock = std::chrono::steady_clock;

constexpr unsigned int kInitialWidth = 1440;
constexpr unsigned int kInitialHeight = 900;
constexpr float kFixedSimulationStep = 1.0f / 120.0f;
constexpr float kStartupRippleDelay = 0.18f;
constexpr int kMaxCatchUpSteps = 4;

struct WaterStrokeState {
    bool active = false;
    glm::vec2 lastHit = glm::vec2(0.0f);
    glm::vec2 filteredVelocity = glm::vec2(0.0f);
};

struct ImpulseCommand {
    glm::vec2 worldXZ = glm::vec2(0.0f);
    glm::vec2 dragVelocity = glm::vec2(0.0f);
    float heightDelta = 0.0f;
    float radius = 0.0f;
};

bool areStructuralSettingsDifferent(const WaterSimulationSettings& lhs, const WaterSimulationSettings& rhs) {
    return lhs.backend != rhs.backend ||
           lhs.particlesX != rhs.particlesX ||
           lhs.particlesY != rhs.particlesY ||
           lhs.particlesZ != rhs.particlesZ ||
           lhs.domainSize != rhs.domainSize ||
           lhs.containerFloorY != rhs.containerFloorY ||
           lhs.containerLipY != rhs.containerLipY ||
           lhs.particleRadius != rhs.particleRadius ||
           lhs.smoothingLength != rhs.smoothingLength ||
           lhs.particleMass != rhs.particleMass ||
           lhs.restDensity != rhs.restDensity;
}

int chooseSnapshotInterval(const WaterSimulationSettings& settings, const SimulationStats& stats) {
    if (settings.qualityPolicy == SimulationQualityPolicy::FavorCorrectness) {
        return 1;
    }

    const bool aggressive =
        settings.qualityPolicy == SimulationQualityPolicy::FavorResponsiveness ||
        settings.qualityPolicy == SimulationQualityPolicy::Auto;
    if (!aggressive) {
        return 1;
    }

    if (stats.particleCount > 15000 || stats.totalStepMs > 9.0f) {
        return 2;
    }
    return 1;
}

bool canInterpolateSnapshots(const WaterRenderSnapshot& from, const WaterRenderSnapshot& to) {
    return from.particles.size() == to.particles.size() && !to.particles.empty() && to.version >= from.version;
}

void interpolateRenderSnapshot(
    const WaterRenderSnapshot& from,
    const WaterRenderSnapshot& to,
    float alpha,
    WaterRenderSnapshot& out
) {
    const float clampedAlpha = glm::clamp(alpha, 0.0f, 1.0f);
    out.particles.resize(to.particles.size());
    for (std::size_t i = 0; i < to.particles.size(); ++i) {
        out.particles[i].position = glm::mix(from.particles[i].position, to.particles[i].position, clampedAlpha);
        out.particles[i].metrics = glm::mix(from.particles[i].metrics, to.particles[i].metrics, clampedAlpha);
    }

    out.particleRadius = glm::mix(from.particleRadius, to.particleRadius, clampedAlpha);
    out.interactionPlaneY = glm::mix(from.interactionPlaneY, to.interactionPlaneY, clampedAlpha);
    out.halfDomainSize = glm::mix(from.halfDomainSize, to.halfDomainSize, clampedAlpha);
    out.containerFloorY = glm::mix(from.containerFloorY, to.containerFloorY, clampedAlpha);
    out.containerLipY = glm::mix(from.containerLipY, to.containerLipY, clampedAlpha);
    out.version = to.version;
}

enum class WaterBehaviorPreset {
    Calm = 0,
    Balanced = 1,
    Lively = 2,
};

void restoreRecommendedWaterSettings(WaterSimulationSettings& settings) {
    const SimulationBackend backend = settings.backend;
    const SimulationQualityPolicy quality = settings.qualityPolicy;
    settings = WaterSimulationSettings{};
    settings.backend = backend;
    settings.qualityPolicy = quality;
}

void applyWaterBehaviorPreset(WaterSimulationSettings& settings, WaterBehaviorPreset preset) {
    const WaterSimulationSettings defaults;

    settings.gravity = defaults.gravity;
    settings.pressureStiffness = defaults.pressureStiffness;
    settings.nearPressureStiffness = defaults.nearPressureStiffness;
    settings.viscosityLinear = defaults.viscosityLinear;
    settings.viscosityQuadratic = defaults.viscosityQuadratic;
    settings.xsphViscosity = defaults.xsphViscosity;
    settings.velocityDamping = defaults.velocityDamping;
    settings.restVelocityDamping = defaults.restVelocityDamping;
    settings.velocityTransfer = defaults.velocityTransfer;
    settings.positionRelaxation = defaults.positionRelaxation;
    settings.tensileCorrection = defaults.tensileCorrection;
    settings.boundaryDamping = defaults.boundaryDamping;
    settings.boundaryRestitution = defaults.boundaryRestitution;
    settings.boundaryFriction = defaults.boundaryFriction;
    settings.substeps = defaults.substeps;
    settings.maxSubsteps = defaults.maxSubsteps;
    settings.pressureIterations = defaults.pressureIterations;
    settings.cflFactor = defaults.cflFactor;
    settings.maxSpeed = defaults.maxSpeed;
    settings.interactionRadius = defaults.interactionRadius;
    settings.interactionForce = defaults.interactionForce;
    settings.interactionLift = defaults.interactionLift;
    settings.interactionMaxSpeed = defaults.interactionMaxSpeed;
    settings.foamDecay = defaults.foamDecay;

    switch (preset) {
    case WaterBehaviorPreset::Calm:
        settings.gravity = 11.2f;
        settings.pressureStiffness = 0.046f;
        settings.nearPressureStiffness = 0.050f;
        settings.viscosityLinear = 0.29f;
        settings.viscosityQuadratic = 0.09f;
        settings.xsphViscosity = 0.23f;
        settings.velocityDamping = 0.72f;
        settings.restVelocityDamping = 2.95f;
        settings.velocityTransfer = 0.55f;
        settings.positionRelaxation = 0.31f;
        settings.tensileCorrection = 0.00012f;
        settings.boundaryDamping = 0.74f;
        settings.boundaryRestitution = 0.012f;
        settings.boundaryFriction = 0.22f;
        settings.cflFactor = 0.28f;
        settings.maxSpeed = 4.1f;
        settings.interactionRadius = 0.46f;
        settings.interactionForce = 0.92f;
        settings.interactionLift = 0.38f;
        settings.interactionMaxSpeed = 2.0f;
        settings.foamDecay = 1.35f;
        break;
    case WaterBehaviorPreset::Balanced:
        break;
    case WaterBehaviorPreset::Lively:
        settings.gravity = 13.8f;
        settings.pressureStiffness = 0.061f;
        settings.nearPressureStiffness = 0.065f;
        settings.viscosityLinear = 0.18f;
        settings.viscosityQuadratic = 0.04f;
        settings.xsphViscosity = 0.12f;
        settings.velocityDamping = 0.48f;
        settings.restVelocityDamping = 1.8f;
        settings.velocityTransfer = 0.68f;
        settings.positionRelaxation = 0.43f;
        settings.tensileCorrection = 0.00022f;
        settings.boundaryDamping = 0.58f;
        settings.boundaryRestitution = 0.035f;
        settings.boundaryFriction = 0.12f;
        settings.cflFactor = 0.36f;
        settings.maxSpeed = 5.2f;
        settings.interactionRadius = 0.40f;
        settings.interactionForce = 1.45f;
        settings.interactionLift = 0.70f;
        settings.interactionMaxSpeed = 2.9f;
        settings.foamDecay = 2.0f;
        break;
    }
}

void stabilizeWaterSettingsForUi(WaterSimulationSettings& settings, bool advancedControlsEnabled) {
    settings.particlesX = std::max(settings.particlesX, 8);
    settings.particlesY = std::max(settings.particlesY, 4);
    settings.particlesZ = std::max(settings.particlesZ, 8);
    settings.maxSubsteps = std::max(settings.maxSubsteps, settings.substeps);
    settings.smoothingLength = std::max(settings.smoothingLength, settings.particleRadius * 2.6f);
    settings.interactionRadius = std::max(settings.interactionRadius, settings.particleRadius * 4.0f);

    if (advancedControlsEnabled) {
        return;
    }

    settings.gravity = glm::clamp(settings.gravity, 9.5f, 16.0f);
    settings.pressureStiffness = glm::clamp(settings.pressureStiffness, 0.038f, 0.072f);
    settings.nearPressureStiffness = glm::clamp(settings.nearPressureStiffness, 0.035f, 0.078f);
    settings.viscosityLinear = glm::clamp(settings.viscosityLinear, 0.12f, 0.34f);
    settings.viscosityQuadratic = glm::clamp(settings.viscosityQuadratic, 0.02f, 0.12f);
    settings.xsphViscosity = glm::clamp(settings.xsphViscosity, 0.10f, 0.28f);
    settings.velocityDamping = glm::clamp(settings.velocityDamping, 0.42f, 0.82f);
    settings.restVelocityDamping = glm::clamp(settings.restVelocityDamping, 1.4f, 3.4f);
    settings.velocityTransfer = glm::clamp(settings.velocityTransfer, 0.48f, 0.74f);
    settings.positionRelaxation = glm::clamp(settings.positionRelaxation, 0.24f, 0.52f);
    settings.tensileCorrection = glm::clamp(settings.tensileCorrection, 0.0f, 0.00035f);
    settings.boundaryDamping = glm::clamp(settings.boundaryDamping, 0.48f, 0.82f);
    settings.boundaryRestitution = glm::clamp(settings.boundaryRestitution, 0.0f, 0.06f);
    settings.boundaryFriction = glm::clamp(settings.boundaryFriction, 0.08f, 0.28f);
    settings.substeps = std::clamp(settings.substeps, 2, 5);
    settings.maxSubsteps = std::clamp(std::max(settings.maxSubsteps, settings.substeps), settings.substeps, 8);
    settings.pressureIterations = std::clamp(settings.pressureIterations, 3, 7);
    settings.cflFactor = glm::clamp(settings.cflFactor, 0.22f, 0.42f);
    settings.maxSpeed = glm::clamp(settings.maxSpeed, 3.0f, 5.6f);
    settings.interactionRadius = glm::clamp(settings.interactionRadius, 0.26f, 0.62f);
    settings.interactionForce = glm::clamp(settings.interactionForce, 0.55f, 1.9f);
    settings.interactionLift = glm::clamp(settings.interactionLift, 0.15f, 0.95f);
    settings.interactionMaxSpeed = glm::clamp(settings.interactionMaxSpeed, 1.2f, 3.4f);
    settings.foamDecay = glm::clamp(settings.foamDecay, 1.0f, 2.4f);
}

class SimulationWorker {
public:
    explicit SimulationWorker(const WaterSimulationSettings& settings)
        : simulation_(settings),
          requestedSettings_(settings),
          appliedSettings_(settings) {
        simulation_.buildRenderSnapshot(publishedSnapshot_);
        publishedStats_ = simulation_.stats();
        hasFreshSnapshot_ = true;
        workerThread_ = std::thread([this] { run(); });
    }

    ~SimulationWorker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }

    void submitSettings(const WaterSimulationSettings& settings) {
        std::lock_guard<std::mutex> lock(mutex_);
        requestedSettings_ = settings;
        settingsDirty_ = true;
    }

    void requestReset() {
        std::lock_guard<std::mutex> lock(mutex_);
        resetRequested_ = true;
    }

    void queueImpulse(const ImpulseCommand& command) {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingImpulses_.push_back(command);
    }

    bool consumeLatest(WaterRenderSnapshot& snapshot, SimulationStats& stats) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats = publishedStats_;
        if (!hasFreshSnapshot_) {
            return false;
        }

        std::swap(snapshot, publishedSnapshot_);
        hasFreshSnapshot_ = false;
        return true;
    }

private:
    void run() {
        auto previousTick = Clock::now();
        float accumulator = 0.0f;
        float accumulatedWallTime = 0.0f;
        float accumulatedSimulatedTime = 0.0f;
        while (true) {
            WaterSimulationSettings nextSettings;
            std::vector<ImpulseCommand> impulses;
            bool shouldReset = false;
            bool shouldExit = false;
            bool applySettings = false;
            bool forcePublish = false;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                shouldExit = !running_;
                nextSettings = requestedSettings_;
                applySettings = settingsDirty_.exchange(false, std::memory_order_relaxed);
                shouldReset = resetRequested_;
                impulses.swap(pendingImpulses_);
                resetRequested_ = false;
            }

            if (shouldExit) {
                return;
            }

            if (applySettings) {
                const bool structuralReset = areStructuralSettingsDifferent(appliedSettings_, nextSettings);
                simulation_.settings() = nextSettings;
                appliedSettings_ = nextSettings;
                if (structuralReset) {
                    simulation_.reset();
                    forcePublish = true;
                    previousTick = Clock::now();
                    accumulator = 0.0f;
                    accumulatedWallTime = 0.0f;
                    accumulatedSimulatedTime = 0.0f;
                }
            }

            if (shouldReset) {
                simulation_.reset();
                forcePublish = true;
                previousTick = Clock::now();
                accumulator = 0.0f;
                accumulatedWallTime = 0.0f;
                accumulatedSimulatedTime = 0.0f;
            }

            for (const ImpulseCommand& command : impulses) {
                simulation_.addImpulse(command.worldXZ, command.dragVelocity, command.heightDelta, command.radius);
            }

            const auto now = Clock::now();
            float wallDelta = std::chrono::duration<float>(now - previousTick).count();
            previousTick = now;
            wallDelta = glm::clamp(wallDelta, 0.0f, 0.1f);
            accumulatedWallTime += wallDelta;
            accumulator = std::min(accumulator + wallDelta, kFixedSimulationStep * static_cast<float>(kMaxCatchUpSteps));

            int executedSteps = 0;
            while (accumulator >= kFixedSimulationStep && executedSteps < kMaxCatchUpSteps) {
                simulation_.step(kFixedSimulationStep);
                accumulator -= kFixedSimulationStep;
                accumulatedSimulatedTime += kFixedSimulationStep;
                ++executedSteps;
            }

            const int snapshotInterval = chooseSnapshotInterval(appliedSettings_, simulation_.stats());
            const float realTimeRatio =
                accumulatedWallTime > 1.0e-5f ? accumulatedSimulatedTime / accumulatedWallTime : 1.0f;
            if (forcePublish ||
                (executedSteps > 0 &&
                 simulation_.stats().frameIndex % static_cast<std::uint64_t>(snapshotInterval) == 0)) {
                const auto snapshotStart = Clock::now();
                simulation_.buildRenderSnapshot(stagingSnapshot_);
                SimulationStats stagedStats = simulation_.stats();
                stagedStats.snapshotMs =
                    std::chrono::duration<float, std::milli>(Clock::now() - snapshotStart).count();
                stagedStats.snapshotInterval = snapshotInterval;
                stagedStats.realTimeRatio = realTimeRatio;

                std::lock_guard<std::mutex> lock(mutex_);
                std::swap(publishedSnapshot_, stagingSnapshot_);
                publishedStats_ = stagedStats;
                hasFreshSnapshot_ = true;
            } else {
                std::lock_guard<std::mutex> lock(mutex_);
                publishedStats_ = simulation_.stats();
                publishedStats_.snapshotInterval = snapshotInterval;
                publishedStats_.realTimeRatio = realTimeRatio;
            }

            if (executedSteps == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    WaterSimulation simulation_;
    WaterSimulationSettings requestedSettings_;
    WaterSimulationSettings appliedSettings_;
    std::thread workerThread_;
    std::mutex mutex_;
    std::vector<ImpulseCommand> pendingImpulses_;
    WaterRenderSnapshot publishedSnapshot_;
    WaterRenderSnapshot stagingSnapshot_;
    SimulationStats publishedStats_;
    bool running_ = true;
    std::atomic<bool> settingsDirty_{false};
    bool resetRequested_ = false;
    bool hasFreshSnapshot_ = false;
};

void seedStartupRipple(SimulationWorker& worker, const WaterRenderSnapshot& snapshot, const WaterSimulationSettings& settings) {
    worker.queueImpulse({
        glm::vec2(-snapshot.halfDomainSize * 0.3f, snapshot.halfDomainSize * 0.16f),
        glm::vec2(0.7f, -0.1f),
        -0.016f,
        settings.interactionRadius * 1.2f
    });
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

bool initGlad() {
    return gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
}

bool intersectWaterPlane(
    const OrbitCamera& camera,
    double cursorX,
    double cursorY,
    float planeY,
    float halfDomain,
    glm::vec3& hitPoint
) {
    const Ray ray = camera.screenPointToRay(cursorX, cursorY);
    if (std::abs(ray.direction.y) < 1.0e-5f) {
        return false;
    }

    const float rayTime = (planeY - ray.origin.y) / ray.direction.y;
    if (rayTime <= 0.0f) {
        return false;
    }

    hitPoint = ray.origin + ray.direction * rayTime;
    return std::abs(hitPoint.x) <= halfDomain && std::abs(hitPoint.z) <= halfDomain;
}

void applyStrokeSegment(
    SimulationWorker& worker,
    const WaterRenderSnapshot& snapshot,
    const WaterSimulationSettings& settings,
    WaterStrokeState& strokeState,
    const glm::vec2& hitXZ,
    float frameTime,
    bool isInitialContact
) {
    if (isInitialContact) {
        worker.queueImpulse({hitXZ, glm::vec2(0.0f), -0.013f, settings.interactionRadius});
        strokeState.lastHit = hitXZ;
        strokeState.filteredVelocity = glm::vec2(0.0f);
        strokeState.active = true;
        return;
    }

    const glm::vec2 delta = hitXZ - strokeState.lastHit;
    const float distance = glm::length(delta);
    glm::vec2 rawVelocity = delta / std::max(frameTime, 1.0e-4f);
    const float rawSpeed = glm::length(rawVelocity);
    if (rawSpeed > settings.interactionMaxSpeed && rawSpeed > 1.0e-5f) {
        rawVelocity *= settings.interactionMaxSpeed / rawSpeed;
    }

    strokeState.filteredVelocity = glm::mix(strokeState.filteredVelocity, rawVelocity, 0.18f);
    const float filteredSpeed = glm::length(strokeState.filteredVelocity);
    const float speedRatio =
        glm::clamp(filteredSpeed / std::max(settings.interactionMaxSpeed, 1.0e-5f), 0.0f, 1.0f);

    const float adaptiveRadius = settings.interactionRadius * (1.0f + speedRatio * 0.5f);
    const float spacing = std::max(adaptiveRadius * 0.35f, snapshot.particleRadius * 1.4f);
    const int stampCount = std::clamp(static_cast<int>(std::ceil(distance / spacing)), 1, 8);
    const float perStampHeight = -0.0105f * (0.8f + speedRatio * 0.55f) / std::sqrt(static_cast<float>(stampCount));

    for (int stampIndex = 1; stampIndex <= stampCount; ++stampIndex) {
        const float alpha = static_cast<float>(stampIndex) / static_cast<float>(stampCount);
        const glm::vec2 stampPosition = glm::mix(strokeState.lastHit, hitXZ, alpha);
        worker.queueImpulse({stampPosition, strokeState.filteredVelocity, perStampHeight, adaptiveRadius});
    }

    strokeState.lastHit = hitXZ;
    strokeState.active = true;
}

bool shouldRunBenchmark(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--benchmark") {
            return true;
        }
    }
    return false;
}

bool shouldUseMetalBackend(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--metal") {
            return true;
        }
    }
    return false;
}

std::string benchmarkSceneFilter(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--benchmark-scene") {
            return argv[i + 1];
        }
    }
    return "all";
}

struct BenchmarkCase {
    const char* label;
    int x;
    int y;
    int z;
};

struct BenchmarkScene {
    const char* name;
    int warmupFrames;
    int measuredFrames;
    void (*applyStep)(WaterSimulation&, const WaterSimulationSettings&, int);
};

void benchmarkNoopStep(WaterSimulation&, const WaterSimulationSettings&, int) {}

void benchmarkRepeatedImpulseStep(WaterSimulation& simulation, const WaterSimulationSettings& settings, int measuredFrame) {
    if (measuredFrame % 24 != 0) {
        return;
    }

    const float halfDomain = settings.domainSize * 0.5f;
    const float direction = (measuredFrame / 24) % 2 == 0 ? 1.0f : -1.0f;
    simulation.addImpulse(
        glm::vec2(direction * halfDomain * 0.35f, 0.0f),
        glm::vec2(direction * 0.9f, 0.0f),
        -0.018f,
        settings.interactionRadius * 1.1f
    );
}

void benchmarkWallSlashStep(WaterSimulation& simulation, const WaterSimulationSettings& settings, int measuredFrame) {
    const int cycleFrame = measuredFrame % 48;
    if (cycleFrame >= 6) {
        return;
    }

    const float halfDomain = settings.domainSize * 0.5f;
    const float alpha = cycleFrame / 5.0f;
    simulation.addImpulse(
        glm::vec2(halfDomain * 0.82f, glm::mix(-halfDomain * 0.45f, halfDomain * 0.45f, alpha)),
        glm::vec2(-settings.interactionMaxSpeed * 0.85f, settings.interactionMaxSpeed * 0.15f),
        -0.012f,
        settings.interactionRadius * 0.95f
    );
}

int runBenchmarks(SimulationBackend backend, const std::string& sceneFilter) {
    const BenchmarkCase cases[] = {
        {"1k", 10, 10, 10},
        {"5k", 17, 17, 18},
        {"20k", 27, 27, 28},
        {"32k", 32, 32, 32},
    };

    const BenchmarkScene scenes[] = {
        {"steady-state", 30, 90, benchmarkNoopStep},
        {"calm-rest", 180, 180, benchmarkNoopStep},
        {"repeated-impulse", 30, 150, benchmarkRepeatedImpulseStep},
        {"wall-slash", 30, 150, benchmarkWallSlashStep},
    };

    std::cout
        << "scene,label,particles,avg_step_ms,avg_grid_ms,avg_density_ms,avg_constraint_ms,avg_divergence_ms,avg_finalize_ms,avg_neighbors,avg_density_error,max_speed,avg_divergence_error\n"
        << std::flush;
    bool matchedScene = false;
    for (const BenchmarkScene& scene : scenes) {
        if (sceneFilter != "all" && sceneFilter != scene.name) {
            continue;
        }
        matchedScene = true;

        for (const BenchmarkCase& benchmarkCase : cases) {
            WaterSimulationSettings settings;
            settings.particlesX = benchmarkCase.x;
            settings.particlesY = benchmarkCase.y;
            settings.particlesZ = benchmarkCase.z;
            settings.qualityPolicy = SimulationQualityPolicy::FavorResponsiveness;
            settings.backend = backend;
            WaterSimulation simulation(settings);

            for (int frame = 0; frame < scene.warmupFrames; ++frame) {
                simulation.step(kFixedSimulationStep);
            }

            float totalStepMs = 0.0f;
            float totalGridMs = 0.0f;
            float totalDensityMs = 0.0f;
            float totalConstraintMs = 0.0f;
            float totalDivergenceMs = 0.0f;
            float totalFinalizeMs = 0.0f;
            float totalNeighbors = 0.0f;
            float totalDensityError = 0.0f;
            float totalDivergenceError = 0.0f;
            float maxSpeed = 0.0f;

            for (int frame = 0; frame < scene.measuredFrames; ++frame) {
                scene.applyStep(simulation, settings, frame);
                simulation.step(kFixedSimulationStep);
                const SimulationStats& stats = simulation.stats();
                totalStepMs += stats.totalStepMs;
                totalGridMs += stats.gridBuildMs;
                totalDensityMs += stats.densityPassMs;
                totalConstraintMs += stats.constraintPassMs;
                totalDivergenceMs += stats.divergencePassMs;
                totalFinalizeMs += stats.finalizeMs;
                totalNeighbors += static_cast<float>(stats.neighborSamples);
                totalDensityError += stats.averageDensityError;
                totalDivergenceError += stats.averageDivergenceError;
                maxSpeed = std::max(maxSpeed, stats.maxSpeed);
            }

            const float frames = static_cast<float>(scene.measuredFrames);
            std::cout
                << scene.name << ','
                << benchmarkCase.label << ','
                << simulation.particleCount() << ','
                << (totalStepMs / frames) << ','
                << (totalGridMs / frames) << ','
                << (totalDensityMs / frames) << ','
                << (totalConstraintMs / frames) << ','
                << (totalDivergenceMs / frames) << ','
                << (totalFinalizeMs / frames) << ','
                << (totalNeighbors / frames) << ','
                << (totalDensityError / frames) << ','
                << maxSpeed << ','
                << (totalDivergenceError / frames) << '\n'
                << std::flush;
        }
    }

    if (!matchedScene) {
        std::cerr
            << "Unknown benchmark scene filter: " << sceneFilter
            << " (expected all, steady-state, calm-rest, repeated-impulse, or wall-slash)"
            << std::endl;
        return 1;
    }

    return 0;
}
} // namespace

int main(int argc, char** argv) {
    const bool useMetalBackend = shouldUseMetalBackend(argc, argv);
    if (shouldRunBenchmark(argc, argv)) {
        return runBenchmarks(useMetalBackend ? SimulationBackend::Metal : SimulationBackend::CPU, benchmarkSceneFilter(argc, argv));
    }

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(kInitialWidth, kInitialHeight, "Fluid Simulation", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    if (!initGlad()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
    ImGui::StyleColorsDark();

    OrbitCamera camera;
    registerCameraCallbacks(window, &camera);

    WaterSimulationSettings uiSettings;
    if (useMetalBackend) {
        uiSettings.backend = SimulationBackend::Metal;
    }
    bool showAdvancedWaterControls = false;
    SimulationWorker simulationWorker(uiSettings);
    WaterRenderSnapshot targetSnapshot;
    WaterRenderSnapshot sourceSnapshot;
    WaterRenderSnapshot displaySnapshot;
    SimulationStats simulationStats;
    simulationWorker.consumeLatest(targetSnapshot, simulationStats);
    sourceSnapshot = targetSnapshot;
    displaySnapshot = targetSnapshot;

    WaterRenderer renderer;
    if (!renderer.initialize(std::string(RESOURCES_PATH), displaySnapshot)) {
        std::cerr << "Failed to initialize renderer resources" << std::endl;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    WaterPalette palette;
    WaterRenderSettings renderSettings;
    renderSettings.basinFloorY = displaySnapshot.containerFloorY;
    renderSettings.basinLipY = displaySnapshot.containerLipY;
    int debugMode = 0;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    double previousTime = glfwGetTime();
    double startupRippleTimer = 0.0;
    double snapshotBlendStartTime = previousTime;
    double snapshotBlendDuration = 0.0;
    bool startupRipplePending = true;
    bool snapshotBlendActive = false;
    WaterStrokeState strokeState;
    float renderUploadMs = 0.0f;

    auto uploadSnapshotToRenderer = [&](const WaterRenderSnapshot& snapshot) {
        const auto uploadStart = Clock::now();
        renderer.updateSurface(snapshot);
        renderUploadMs = std::chrono::duration<float, std::milli>(Clock::now() - uploadStart).count();
        renderSettings.basinFloorY = snapshot.containerFloorY;
        renderSettings.basinLipY = snapshot.containerLipY;
    };

    while (!glfwWindowShouldClose(window)) {
        const double currentTime = glfwGetTime();
        float frameTime = static_cast<float>(currentTime - previousTime);
        previousTime = currentTime;
        frameTime = std::clamp(frameTime, 0.0f, 0.05f);
        startupRippleTimer += frameTime;

        glfwPollEvents();
        processInput(window);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        camera.setViewport(windowWidth, windowHeight);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiIO& io = ImGui::GetIO();
        camera.update(window, frameTime, io.WantCaptureMouse);

        if (startupRipplePending && startupRippleTimer >= kStartupRippleDelay) {
            seedStartupRipple(simulationWorker, displaySnapshot, uiSettings);
            startupRipplePending = false;
        }

        bool hoverValid = false;
        glm::vec3 hoverPoint(0.0f);
        if (!io.WantCaptureMouse) {
            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetCursorPos(window, &cursorX, &cursorY);
            hoverValid = intersectWaterPlane(
                camera,
                cursorX,
                cursorY,
                displaySnapshot.interactionPlaneY,
                displaySnapshot.halfDomainSize,
                hoverPoint
            );
        }

        if (debugMode == 4 && hoverValid) {
            renderer.setInteractionIndicator(
                true,
                hoverPoint,
                uiSettings.interactionRadius,
                glm::clamp(
                    glm::length(strokeState.filteredVelocity) / std::max(uiSettings.interactionMaxSpeed, 1.0e-5f),
                    0.0f,
                    1.0f
                )
            );
        } else {
            renderer.setInteractionIndicator(false, glm::vec3(0.0f), 0.0f, 0.0f);
        }

        if (!io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && hoverValid) {
            applyStrokeSegment(
                simulationWorker,
                displaySnapshot,
                uiSettings,
                strokeState,
                glm::vec2(hoverPoint.x, hoverPoint.z),
                frameTime,
                !strokeState.active
            );
        } else {
            strokeState.active = false;
            strokeState.filteredVelocity = glm::vec2(0.0f);
        }

        if (simulationWorker.consumeLatest(targetSnapshot, simulationStats)) {
            const std::uint64_t versionDelta =
                targetSnapshot.version > displaySnapshot.version ? targetSnapshot.version - displaySnapshot.version : 1;
            if (versionDelta > 1 && canInterpolateSnapshots(displaySnapshot, targetSnapshot)) {
                sourceSnapshot = displaySnapshot;
                snapshotBlendDuration = static_cast<double>(versionDelta) * static_cast<double>(kFixedSimulationStep);
                snapshotBlendStartTime =
                    currentTime - std::min<double>(static_cast<double>(frameTime), snapshotBlendDuration);
                snapshotBlendActive = true;
            } else {
                snapshotBlendActive = false;
                displaySnapshot = targetSnapshot;
                uploadSnapshotToRenderer(displaySnapshot);
            }
        }

        if (snapshotBlendActive) {
            const double elapsedBlendTime = currentTime - snapshotBlendStartTime;
            const float alpha = static_cast<float>(
                std::clamp(
                    snapshotBlendDuration > 0.0 ? elapsedBlendTime / snapshotBlendDuration : 1.0,
                    0.0,
                    1.0
                )
            );
            interpolateRenderSnapshot(sourceSnapshot, targetSnapshot, alpha, displaySnapshot);
            uploadSnapshotToRenderer(displaySnapshot);
            if (alpha >= 1.0f - 1.0e-4f) {
                snapshotBlendActive = false;
            }
        }

        bool settingsChanged = false;
        bool requestWaterReset = false;

        ImGui::Begin("Water Controls");
        ImGui::Text("Render FPS %.1f", io.Framerate);
        ImGui::Text("Particles %zu", simulationStats.particleCount);
        ImGui::Text("Sim step %.2f ms", simulationStats.totalStepMs);
        ImGui::Text("Grid %.2f ms  Density %.2f ms", simulationStats.gridBuildMs, simulationStats.densityPassMs);
        ImGui::Text(
            "Constraint %.2f ms  Divergence %.2f ms  Finalize %.2f ms",
            simulationStats.constraintPassMs,
            simulationStats.divergencePassMs,
            simulationStats.finalizeMs
        );
        ImGui::Text(
            "Snapshot %.2f ms  Upload %.2f ms",
            simulationStats.snapshotMs,
            renderUploadMs
        );
        ImGui::Text(
            "Density err %.3f  Divergence err %.3f  Real-time %.2fx",
            simulationStats.averageDensityError,
            simulationStats.averageDivergenceError,
            simulationStats.realTimeRatio
        );
        ImGui::Text(
            "Publish every %d step%s",
            simulationStats.snapshotInterval,
            simulationStats.snapshotInterval == 1 ? "" : "s"
        );
        ImGui::Separator();
        ImGui::TextUnformatted("Camera");
        ImGui::TextUnformatted("Right drag orbit");
        ImGui::TextUnformatted("Middle drag pan");
        ImGui::TextUnformatted("Scroll zoom");
        ImGui::TextUnformatted("Left drag disturb fluid");
        ImGui::Separator();
        ImGui::TextUnformatted("Starting Point");
        ImGui::TextWrapped("Use the preset buttons first. Guided mode keeps the sliders in a safer range so the water stays usable.");
        if (ImGui::Button("Restore recommended")) {
            restoreRecommendedWaterSettings(uiSettings);
            settingsChanged = true;
            requestWaterReset = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Calm preset")) {
            applyWaterBehaviorPreset(uiSettings, WaterBehaviorPreset::Calm);
            settingsChanged = true;
            requestWaterReset = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Balanced preset")) {
            applyWaterBehaviorPreset(uiSettings, WaterBehaviorPreset::Balanced);
            settingsChanged = true;
            requestWaterReset = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Lively preset")) {
            applyWaterBehaviorPreset(uiSettings, WaterBehaviorPreset::Lively);
            settingsChanged = true;
            requestWaterReset = true;
        }
        ImGui::Checkbox("Advanced solver controls", &showAdvancedWaterControls);
        ImGui::Separator();
        ImGui::TextUnformatted("Structure");
        settingsChanged |= ImGui::SliderInt("Particles X", &uiSettings.particlesX, 8, 48);
        settingsChanged |= ImGui::SliderInt("Particles Y", &uiSettings.particlesY, 4, 36);
        settingsChanged |= ImGui::SliderInt("Particles Z", &uiSettings.particlesZ, 8, 48);
        settingsChanged |= ImGui::SliderFloat("Domain size", &uiSettings.domainSize, 3.0f, 10.0f);
        if (showAdvancedWaterControls) {
            settingsChanged |= ImGui::SliderFloat("Particle radius", &uiSettings.particleRadius, 0.035f, 0.12f);
            settingsChanged |= ImGui::SliderFloat("Smoothing length", &uiSettings.smoothingLength, 0.10f, 0.32f);
            settingsChanged |= ImGui::SliderFloat("Particle mass", &uiSettings.particleMass, 0.004f, 0.04f);
            settingsChanged |= ImGui::SliderFloat("Rest density", &uiSettings.restDensity, 2.0f, 10.0f);
        } else {
            ImGui::TextDisabled(
                "Guided mode keeps particle radius, kernel size, mass, and rest density on the recommended setup."
            );
        }
        ImGui::Separator();
        if (!showAdvancedWaterControls) {
            ImGui::TextUnformatted("Guided Simulation");
            settingsChanged |= ImGui::SliderFloat("Gravity", &uiSettings.gravity, 9.5f, 16.0f);
            settingsChanged |= ImGui::SliderFloat("Pressure", &uiSettings.pressureStiffness, 0.038f, 0.072f);
            settingsChanged |= ImGui::SliderFloat("Near pressure", &uiSettings.nearPressureStiffness, 0.035f, 0.078f);
            settingsChanged |= ImGui::SliderFloat("Linear viscosity", &uiSettings.viscosityLinear, 0.12f, 0.34f);
            settingsChanged |= ImGui::SliderFloat("Quadratic viscosity", &uiSettings.viscosityQuadratic, 0.02f, 0.12f);
            settingsChanged |= ImGui::SliderFloat("XSPH viscosity", &uiSettings.xsphViscosity, 0.10f, 0.28f);
            settingsChanged |= ImGui::SliderFloat("Velocity damping", &uiSettings.velocityDamping, 0.42f, 0.82f);
            settingsChanged |= ImGui::SliderFloat("Rest damping", &uiSettings.restVelocityDamping, 1.4f, 3.4f);
            settingsChanged |= ImGui::SliderFloat("Force radius", &uiSettings.interactionRadius, 0.26f, 0.62f);
            settingsChanged |= ImGui::SliderFloat("Force strength", &uiSettings.interactionForce, 0.55f, 1.9f);
            settingsChanged |= ImGui::SliderFloat("Lift amount", &uiSettings.interactionLift, 0.15f, 0.95f);
            settingsChanged |= ImGui::SliderFloat("Max swipe speed", &uiSettings.interactionMaxSpeed, 1.2f, 3.4f);
            settingsChanged |= ImGui::SliderInt("Solver iterations", &uiSettings.pressureIterations, 3, 7);
            settingsChanged |= ImGui::SliderInt("Substeps", &uiSettings.substeps, 2, 5);
            settingsChanged |= ImGui::SliderFloat("Foam decay", &uiSettings.foamDecay, 1.0f, 2.4f);
        } else {
            ImGui::TextUnformatted("Advanced Simulation");
            settingsChanged |= ImGui::SliderFloat("Gravity", &uiSettings.gravity, 4.0f, 28.0f);
            settingsChanged |= ImGui::SliderFloat("Pressure", &uiSettings.pressureStiffness, 0.01f, 0.18f);
            settingsChanged |= ImGui::SliderFloat("Near pressure", &uiSettings.nearPressureStiffness, 0.0f, 0.25f);
            settingsChanged |= ImGui::SliderFloat("Linear viscosity", &uiSettings.viscosityLinear, 0.0f, 0.5f);
            settingsChanged |= ImGui::SliderFloat("Quadratic viscosity", &uiSettings.viscosityQuadratic, 0.0f, 0.4f);
            settingsChanged |= ImGui::SliderFloat("XSPH viscosity", &uiSettings.xsphViscosity, 0.0f, 0.45f);
            settingsChanged |= ImGui::SliderFloat("Velocity damping", &uiSettings.velocityDamping, 0.0f, 1.6f);
            settingsChanged |= ImGui::SliderFloat("Rest damping", &uiSettings.restVelocityDamping, 0.0f, 4.0f);
            settingsChanged |= ImGui::SliderFloat("Velocity transfer", &uiSettings.velocityTransfer, 0.1f, 1.0f);
            settingsChanged |= ImGui::SliderFloat("Position relax", &uiSettings.positionRelaxation, 0.05f, 0.9f);
            settingsChanged |= ImGui::SliderFloat("Tensile correction", &uiSettings.tensileCorrection, 0.0f, 0.001f);
            settingsChanged |= ImGui::SliderFloat("Boundary damping", &uiSettings.boundaryDamping, 0.0f, 1.0f);
            settingsChanged |= ImGui::SliderFloat("Boundary restitution", &uiSettings.boundaryRestitution, 0.0f, 0.18f);
            settingsChanged |= ImGui::SliderFloat("Boundary friction", &uiSettings.boundaryFriction, 0.0f, 0.45f);
            settingsChanged |= ImGui::SliderInt("Substeps", &uiSettings.substeps, 1, 6);
            settingsChanged |= ImGui::SliderInt("Max substeps", &uiSettings.maxSubsteps, 1, 10);
            settingsChanged |= ImGui::SliderInt("Solver iterations", &uiSettings.pressureIterations, 1, 10);
            settingsChanged |= ImGui::SliderFloat("CFL factor", &uiSettings.cflFactor, 0.15f, 0.9f);
            settingsChanged |= ImGui::SliderFloat("Max speed", &uiSettings.maxSpeed, 1.0f, 8.0f);
            settingsChanged |= ImGui::SliderFloat("Force radius", &uiSettings.interactionRadius, 0.18f, 1.0f);
            settingsChanged |= ImGui::SliderFloat("Force strength", &uiSettings.interactionForce, 0.2f, 3.0f);
            settingsChanged |= ImGui::SliderFloat("Lift amount", &uiSettings.interactionLift, 0.0f, 2.0f);
            settingsChanged |= ImGui::SliderFloat("Max swipe speed", &uiSettings.interactionMaxSpeed, 0.8f, 6.0f);
            settingsChanged |= ImGui::SliderFloat("Foam decay", &uiSettings.foamDecay, 0.2f, 3.0f);
        }
        const char* qualityModes[] = {"Balanced", "Responsive", "Correct", "Auto"};
        int qualityMode = static_cast<int>(uiSettings.qualityPolicy);
        if (ImGui::Combo("Quality policy", &qualityMode, qualityModes, IM_ARRAYSIZE(qualityModes))) {
            uiSettings.qualityPolicy = static_cast<SimulationQualityPolicy>(qualityMode);
            settingsChanged = true;
        }
#ifdef __APPLE__
        const char* backendModes[] = {"CPU", "Metal"};
#else
        const char* backendModes[] = {"CPU", "Metal (Unavailable)"};
#endif
        int backendMode = static_cast<int>(uiSettings.backend);
        if (ImGui::Combo("Backend", &backendMode, backendModes, IM_ARRAYSIZE(backendModes))) {
            uiSettings.backend = static_cast<SimulationBackend>(backendMode);
            settingsChanged = true;
        }
        ImGui::Separator();
        const char* debugModes[] = {"Shaded particles", "Density", "Velocity", "Pressure", "Interaction"};
        ImGui::Combo("Debug view", &debugMode, debugModes, IM_ARRAYSIZE(debugModes));
        ImGui::ColorEdit3("Shallow color", &palette.shallowColor.x);
        ImGui::ColorEdit3("Deep color", &palette.deepColor.x);
        ImGui::ColorEdit3("Foam color", &palette.foamColor.x);
        ImGui::ColorEdit3("Basin color", &palette.basinColor.x);
        ImGui::ColorEdit3("Accent color", &palette.accentColor.x);
        ImGui::SliderFloat("Water alpha", &renderSettings.waterAlpha, 0.45f, 0.95f);
        ImGui::SliderFloat("Particle scale", &renderSettings.particleScale, 0.8f, 2.6f);
        ImGui::SliderFloat("Ambient", &renderSettings.ambientStrength, 0.1f, 0.7f);
        ImGui::SliderFloat("Specular", &renderSettings.specularStrength, 0.0f, 0.5f);
        ImGui::SliderFloat("Fresnel", &renderSettings.fresnelStrength, 0.0f, 0.45f);
        if (ImGui::Button("Reset water")) {
            requestWaterReset = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Print benchmark hint")) {
            std::cout << "Run ./build-perf/FluidSimulation --benchmark for CSV benchmark output." << std::endl;
        }
        ImGui::End();

        if (settingsChanged) {
            stabilizeWaterSettingsForUi(uiSettings, showAdvancedWaterControls);
            simulationWorker.submitSettings(uiSettings);
        }

        if (requestWaterReset) {
            simulationWorker.requestReset();
            startupRippleTimer = 0.0;
            startupRipplePending = true;
            strokeState.active = false;
            strokeState.filteredVelocity = glm::vec2(0.0f);
        }

        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(palette.skyColor.r, palette.skyColor.g, palette.skyColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderer.render(
            camera.getViewMatrix(),
            camera.getProjectionMatrix(),
            camera.getPosition(),
            palette,
            renderSettings,
            debugMode,
            static_cast<float>(currentTime),
            framebufferHeight
        );

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
