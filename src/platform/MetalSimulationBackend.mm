#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <simd/simd.h>

#include "platform/MetalSimulationBackend.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#ifndef RESOURCES_PATH
#define RESOURCES_PATH "../resources/"
#endif

namespace {
using Clock = std::chrono::steady_clock;

constexpr float kEpsilon = 1.0e-6f;
constexpr float kSpawnSpacingScale = 1.95f;
constexpr float kMinDeltaTime = 1.0e-4f;
constexpr float kMaxDeltaTime = 1.0f / 24.0f;

float toMilliseconds(const Clock::duration& duration) {
    return std::chrono::duration<float, std::milli>(duration).count();
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

struct MetalSimParams {
    std::uint32_t particleCount = 0;
    std::uint32_t gridResolutionX = 0;
    std::uint32_t gridResolutionY = 0;
    std::uint32_t gridResolutionZ = 0;
    float deltaTime = 0.0f;
    float smoothingLength = 0.0f;
    float smoothingLengthSquared = 0.0f;
    float particleMass = 0.0f;
    float restDensity = 0.0f;
    float pressureStiffness = 0.0f;
    float nearPressureStiffness = 0.0f;
    float xsphViscosity = 0.0f;
    float velocityDamping = 0.0f;
    float restVelocityDamping = 0.0f;
    float velocityTransfer = 0.0f;
    float positionRelaxation = 0.0f;
    float tensileCorrection = 0.0f;
    float boundaryDamping = 0.0f;
    float boundaryRestitution = 0.0f;
    float boundaryFriction = 0.0f;
    float gravity = 0.0f;
    float maxSpeed = 0.0f;
    float maxSpeedSquared = 0.0f;
    float halfDomain = 0.0f;
    float containerFloorY = 0.0f;
    float containerLipY = 0.0f;
    float particleRadius = 0.0f;
    float interactionPlaneY = 0.0f;
};

void sanitizeSettings(WaterSimulationSettings& settings) {
    settings.particlesX = std::max(settings.particlesX, 4);
    settings.particlesY = std::max(settings.particlesY, 2);
    settings.particlesZ = std::max(settings.particlesZ, 4);
    settings.domainSize = std::max(settings.domainSize, 2.0f);
    settings.particleRadius = std::max(settings.particleRadius, 0.02f);
    settings.smoothingLength = std::max(settings.smoothingLength, settings.particleRadius * 2.4f);
    settings.particleMass = std::max(settings.particleMass, 0.001f);
    settings.restDensity = std::max(settings.restDensity, 0.5f);
    settings.pressureStiffness = std::max(settings.pressureStiffness, 0.001f);
    settings.nearPressureStiffness = std::max(settings.nearPressureStiffness, 0.0f);
    settings.xsphViscosity = glm::clamp(settings.xsphViscosity, 0.0f, 1.0f);
    settings.velocityTransfer = glm::clamp(settings.velocityTransfer, 0.0f, 1.0f);
    settings.positionRelaxation = glm::clamp(settings.positionRelaxation, 0.05f, 1.0f);
    settings.tensileCorrection = glm::clamp(settings.tensileCorrection, 0.0f, 0.01f);
    settings.boundaryDamping = glm::clamp(settings.boundaryDamping, 0.0f, 1.0f);
    settings.boundaryRestitution = glm::clamp(settings.boundaryRestitution, 0.0f, 0.4f);
    settings.boundaryFriction = glm::clamp(settings.boundaryFriction, 0.0f, 1.0f);
    settings.substeps = std::max(settings.substeps, 1);
    settings.pressureIterations = std::max(settings.pressureIterations, 1);
    settings.cflFactor = glm::clamp(settings.cflFactor, 0.1f, 0.95f);
    settings.maxSubsteps = std::max(settings.maxSubsteps, settings.substeps);
    settings.maxSpeed = std::max(settings.maxSpeed, 0.5f);
    settings.interactionRadius = std::max(settings.interactionRadius, settings.particleRadius * 2.5f);
    settings.interactionMaxSpeed = std::max(settings.interactionMaxSpeed, 0.5f);
}
} // namespace

struct MetalSimulationBackend::Impl {
    bool available = false;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLLibrary> library = nil;
    id<MTLComputePipelineState> integratePipeline = nil;
    id<MTLComputePipelineState> assignCellsPipeline = nil;
    id<MTLComputePipelineState> scatterSortedPipeline = nil;
    id<MTLComputePipelineState> densityLambdaPipeline = nil;
    id<MTLComputePipelineState> correctionPipeline = nil;
    id<MTLComputePipelineState> applyCorrectionPipeline = nil;
    id<MTLComputePipelineState> finalizePipeline = nil;

    id<MTLBuffer> paramsBuffer = nil;
    id<MTLBuffer> positionsBuffer = nil;
    id<MTLBuffer> predictedBuffer = nil;
    id<MTLBuffer> velocitiesBuffer = nil;
    id<MTLBuffer> correctionsBuffer = nil;
    id<MTLBuffer> densitiesBuffer = nil;
    id<MTLBuffer> lambdasBuffer = nil;
    id<MTLBuffer> metricsBuffer = nil;
    id<MTLBuffer> interactionHeatBuffer = nil;
    id<MTLBuffer> foamBuffer = nil;
    id<MTLBuffer> particleCellsBuffer = nil;
    id<MTLBuffer> cellCountsBuffer = nil;
    id<MTLBuffer> cellStartsBuffer = nil;
    id<MTLBuffer> cellWriteHeadsBuffer = nil;
    id<MTLBuffer> sortedParticleIndicesBuffer = nil;
    id<MTLBuffer> neighborCounterBuffer = nil;

    WaterSimulationSettings settings;
    SimulationStats stats;
    MetalSimParams params;
    float halfDomain = 0.0f;
    float interactionPlaneY = 0.0f;
    int gridResolutionX = 0;
    int gridResolutionY = 0;
    int gridResolutionZ = 0;
    std::size_t particleCount = 0;
    std::size_t cellCount = 0;

    Impl() {
        device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            return;
        }

        commandQueue = [device newCommandQueue];
        if (commandQueue == nil) {
            return;
        }

        @autoreleasepool {
            NSString* sourcePath = [NSString stringWithUTF8String:(std::string(RESOURCES_PATH) + "shaders/water_simulation.metal").c_str()];
            NSError* error = nil;
            NSString* source =
                [NSString stringWithContentsOfFile:sourcePath encoding:NSUTF8StringEncoding error:&error];
            if (source == nil) {
                if (error != nil) {
                    std::cerr << "Failed to read Metal source: " << [[error localizedDescription] UTF8String] << std::endl;
                }
                return;
            }

            library = [device newLibraryWithSource:source options:nil error:&error];
            if (library == nil) {
                if (error != nil) {
                    std::cerr << "Failed to compile Metal library: " << [[error localizedDescription] UTF8String] << std::endl;
                }
                return;
            }

            integratePipeline = createPipeline(@"integratePredictedKernel");
            assignCellsPipeline = createPipeline(@"assignCellsKernel");
            scatterSortedPipeline = createPipeline(@"scatterSortedKernel");
            densityLambdaPipeline = createPipeline(@"computeDensityLambdaKernel");
            correctionPipeline = createPipeline(@"computePositionCorrectionsKernel");
            applyCorrectionPipeline = createPipeline(@"applyCorrectionsKernel");
            finalizePipeline = createPipeline(@"finalizeKernel");
        }

        available =
            integratePipeline != nil &&
            assignCellsPipeline != nil &&
            scatterSortedPipeline != nil &&
            densityLambdaPipeline != nil &&
            correctionPipeline != nil &&
            applyCorrectionPipeline != nil &&
            finalizePipeline != nil;
    }

    id<MTLComputePipelineState> createPipeline(NSString* functionName) {
        NSError* error = nil;
        id<MTLFunction> function = [library newFunctionWithName:functionName];
        if (function == nil) {
            std::cerr << "Missing Metal function: " << [functionName UTF8String] << std::endl;
            return nil;
        }
        id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
        if (pipeline == nil && error != nil) {
            std::cerr << "Failed to create Metal pipeline " << [functionName UTF8String]
                      << ": " << [[error localizedDescription] UTF8String] << std::endl;
        }
        return pipeline;
    }

    void updateDerivedConfig() {
        halfDomain = settings.domainSize * 0.5f;
        gridResolutionX = std::max(1, static_cast<int>(std::ceil((settings.domainSize) / settings.smoothingLength)) + 1);
        gridResolutionY = std::max(
            1,
            static_cast<int>(std::ceil((settings.containerLipY - settings.containerFloorY) / settings.smoothingLength)) + 1
        );
        gridResolutionZ = gridResolutionX;
        particleCount =
            static_cast<std::size_t>(settings.particlesX) *
            static_cast<std::size_t>(settings.particlesY) *
            static_cast<std::size_t>(settings.particlesZ);
        cellCount =
            static_cast<std::size_t>(gridResolutionX) *
            static_cast<std::size_t>(gridResolutionY) *
            static_cast<std::size_t>(gridResolutionZ);
    }

    void allocateBuffer(id<MTLBuffer> __strong* buffer, std::size_t byteCount) {
        if (*buffer != nil && [*buffer length] >= byteCount) {
            return;
        }

        *buffer = [device newBufferWithLength:byteCount options:MTLResourceStorageModeShared];
    }

    void allocateBuffers() {
        allocateBuffer(&paramsBuffer, sizeof(MetalSimParams));
        allocateBuffer(&positionsBuffer, particleCount * sizeof(simd_float4));
        allocateBuffer(&predictedBuffer, particleCount * sizeof(simd_float4));
        allocateBuffer(&velocitiesBuffer, particleCount * sizeof(simd_float4));
        allocateBuffer(&correctionsBuffer, particleCount * sizeof(simd_float4));
        allocateBuffer(&densitiesBuffer, particleCount * sizeof(float));
        allocateBuffer(&lambdasBuffer, particleCount * sizeof(float));
        allocateBuffer(&metricsBuffer, particleCount * sizeof(simd_float4));
        allocateBuffer(&interactionHeatBuffer, particleCount * sizeof(float));
        allocateBuffer(&foamBuffer, particleCount * sizeof(float));
        allocateBuffer(&particleCellsBuffer, particleCount * sizeof(std::uint32_t));
        allocateBuffer(&cellCountsBuffer, cellCount * sizeof(std::uint32_t));
        allocateBuffer(&cellStartsBuffer, (cellCount + 1) * sizeof(std::uint32_t));
        allocateBuffer(&cellWriteHeadsBuffer, cellCount * sizeof(std::uint32_t));
        allocateBuffer(&sortedParticleIndicesBuffer, particleCount * sizeof(std::uint32_t));
        allocateBuffer(&neighborCounterBuffer, sizeof(std::uint32_t));
    }

    void refreshParams(float deltaTime) {
        params.particleCount = static_cast<std::uint32_t>(particleCount);
        params.gridResolutionX = static_cast<std::uint32_t>(gridResolutionX);
        params.gridResolutionY = static_cast<std::uint32_t>(gridResolutionY);
        params.gridResolutionZ = static_cast<std::uint32_t>(gridResolutionZ);
        params.deltaTime = deltaTime;
        params.smoothingLength = settings.smoothingLength;
        params.smoothingLengthSquared = settings.smoothingLength * settings.smoothingLength;
        params.particleMass = settings.particleMass;
        params.restDensity = settings.restDensity;
        params.pressureStiffness = settings.pressureStiffness;
        params.nearPressureStiffness = settings.nearPressureStiffness;
        params.xsphViscosity = settings.xsphViscosity;
        params.velocityDamping = settings.velocityDamping;
        params.restVelocityDamping = settings.restVelocityDamping;
        params.velocityTransfer = settings.velocityTransfer;
        params.positionRelaxation = settings.positionRelaxation;
        params.tensileCorrection = settings.tensileCorrection;
        params.boundaryDamping = settings.boundaryDamping;
        params.boundaryRestitution = settings.boundaryRestitution;
        params.boundaryFriction = settings.boundaryFriction;
        params.gravity = settings.gravity;
        params.maxSpeed = settings.maxSpeed;
        params.maxSpeedSquared = settings.maxSpeed * settings.maxSpeed;
        params.halfDomain = halfDomain;
        params.containerFloorY = settings.containerFloorY;
        params.containerLipY = settings.containerLipY;
        params.particleRadius = settings.particleRadius;
        params.interactionPlaneY = interactionPlaneY;
        std::memcpy([paramsBuffer contents], &params, sizeof(params));
    }

    void dispatch1D(id<MTLComputeCommandEncoder> encoder, id<MTLComputePipelineState> pipeline, std::size_t count) {
        [encoder setComputePipelineState:pipeline];
        const NSUInteger threadWidth = std::max<NSUInteger>(1, [pipeline threadExecutionWidth]);
        const NSUInteger threadsPerGroup =
            std::min<NSUInteger>(std::max<NSUInteger>(threadWidth, 64), [pipeline maxTotalThreadsPerThreadgroup]);
        [encoder dispatchThreads:MTLSizeMake(count, 1, 1)
          threadsPerThreadgroup:MTLSizeMake(threadsPerGroup, 1, 1)];
    }

    void waitForCommandBuffer(id<MTLCommandBuffer> commandBuffer) {
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
        if ([commandBuffer status] == MTLCommandBufferStatusError && [commandBuffer error] != nil) {
            std::cerr << "Metal command buffer failed: "
                      << [[[commandBuffer error] localizedDescription] UTF8String] << std::endl;
        }
    }

    void rebuildGrid(float deltaTime) {
        refreshParams(deltaTime);

        std::memset([cellCountsBuffer contents], 0, cellCount * sizeof(std::uint32_t));

        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:0];
        [encoder setBuffer:predictedBuffer offset:0 atIndex:1];
        [encoder setBuffer:particleCellsBuffer offset:0 atIndex:2];
        [encoder setBuffer:cellCountsBuffer offset:0 atIndex:3];
        dispatch1D(encoder, assignCellsPipeline, particleCount);
        [encoder endEncoding];
        waitForCommandBuffer(commandBuffer);

        auto* counts = static_cast<std::uint32_t*>([cellCountsBuffer contents]);
        auto* starts = static_cast<std::uint32_t*>([cellStartsBuffer contents]);
        starts[0] = 0;
        stats.activeCells = 0;
        for (std::size_t i = 0; i < cellCount; ++i) {
            if (counts[i] > 0) {
                ++stats.activeCells;
            }
            starts[i + 1] = starts[i] + counts[i];
        }
        std::memcpy([cellWriteHeadsBuffer contents], starts, cellCount * sizeof(std::uint32_t));

        commandBuffer = [commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:0];
        [encoder setBuffer:particleCellsBuffer offset:0 atIndex:1];
        [encoder setBuffer:cellWriteHeadsBuffer offset:0 atIndex:2];
        [encoder setBuffer:sortedParticleIndicesBuffer offset:0 atIndex:3];
        dispatch1D(encoder, scatterSortedPipeline, particleCount);
        [encoder endEncoding];
        waitForCommandBuffer(commandBuffer);
    }

    void runIntegrate(float deltaTime) {
        refreshParams(deltaTime);
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:0];
        [encoder setBuffer:positionsBuffer offset:0 atIndex:1];
        [encoder setBuffer:predictedBuffer offset:0 atIndex:2];
        [encoder setBuffer:velocitiesBuffer offset:0 atIndex:3];
        dispatch1D(encoder, integratePipeline, particleCount);
        [encoder endEncoding];
        waitForCommandBuffer(commandBuffer);
    }

    std::uint32_t resetNeighborCounter() {
        auto* counter = static_cast<std::uint32_t*>([neighborCounterBuffer contents]);
        *counter = 0u;
        return 0u;
    }

    std::uint32_t fetchNeighborCounter() const {
        return *static_cast<const std::uint32_t*>([neighborCounterBuffer contents]);
    }

    void runDensityLambda(float deltaTime) {
        refreshParams(deltaTime);
        resetNeighborCounter();
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:0];
        [encoder setBuffer:predictedBuffer offset:0 atIndex:1];
        [encoder setBuffer:cellStartsBuffer offset:0 atIndex:2];
        [encoder setBuffer:sortedParticleIndicesBuffer offset:0 atIndex:3];
        [encoder setBuffer:densitiesBuffer offset:0 atIndex:4];
        [encoder setBuffer:lambdasBuffer offset:0 atIndex:5];
        [encoder setBuffer:neighborCounterBuffer offset:0 atIndex:6];
        dispatch1D(encoder, densityLambdaPipeline, particleCount);
        [encoder endEncoding];
        waitForCommandBuffer(commandBuffer);
        stats.neighborSamples += fetchNeighborCounter();
    }

    void runCorrections(float deltaTime) {
        refreshParams(deltaTime);
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:0];
        [encoder setBuffer:predictedBuffer offset:0 atIndex:1];
        [encoder setBuffer:cellStartsBuffer offset:0 atIndex:2];
        [encoder setBuffer:sortedParticleIndicesBuffer offset:0 atIndex:3];
        [encoder setBuffer:lambdasBuffer offset:0 atIndex:4];
        [encoder setBuffer:correctionsBuffer offset:0 atIndex:5];
        dispatch1D(encoder, correctionPipeline, particleCount);

        [encoder setBuffer:paramsBuffer offset:0 atIndex:0];
        [encoder setBuffer:predictedBuffer offset:0 atIndex:1];
        [encoder setBuffer:correctionsBuffer offset:0 atIndex:2];
        dispatch1D(encoder, applyCorrectionPipeline, particleCount);
        [encoder endEncoding];
        waitForCommandBuffer(commandBuffer);
    }

    void runFinalize(float deltaTime) {
        refreshParams(deltaTime);
        resetNeighborCounter();
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        [encoder setBuffer:paramsBuffer offset:0 atIndex:0];
        [encoder setBuffer:positionsBuffer offset:0 atIndex:1];
        [encoder setBuffer:predictedBuffer offset:0 atIndex:2];
        [encoder setBuffer:velocitiesBuffer offset:0 atIndex:3];
        [encoder setBuffer:cellStartsBuffer offset:0 atIndex:4];
        [encoder setBuffer:sortedParticleIndicesBuffer offset:0 atIndex:5];
        [encoder setBuffer:densitiesBuffer offset:0 atIndex:6];
        [encoder setBuffer:metricsBuffer offset:0 atIndex:7];
        [encoder setBuffer:interactionHeatBuffer offset:0 atIndex:8];
        [encoder setBuffer:foamBuffer offset:0 atIndex:9];
        [encoder setBuffer:neighborCounterBuffer offset:0 atIndex:10];
        dispatch1D(encoder, finalizePipeline, particleCount);
        [encoder endEncoding];
        waitForCommandBuffer(commandBuffer);
        stats.neighborSamples += fetchNeighborCounter();
    }

    void updateDerivedStateFromPositions() {
        const auto* positions = static_cast<const simd_float4*>([positionsBuffer contents]);
        if (particleCount == 0) {
            interactionPlaneY = settings.containerFloorY + settings.particleRadius * 3.0f;
            return;
        }

        float averageY = 0.0f;
        float maxY = positions[0].y;
        for (std::size_t i = 0; i < particleCount; ++i) {
            averageY += positions[i].y;
            maxY = std::max(maxY, positions[i].y);
        }
        averageY /= static_cast<float>(particleCount);
        interactionPlaneY = glm::clamp(
            glm::mix(averageY + settings.smoothingLength * 0.8f, maxY - settings.particleRadius * 0.4f, 0.28f),
            settings.containerFloorY + settings.particleRadius * 2.3f,
            settings.containerLipY - settings.particleRadius * 2.1f
        );
    }

    void reduceStats() {
        const auto* densities = static_cast<const float*>([densitiesBuffer contents]);
        const auto* velocities = static_cast<const simd_float4*>([velocitiesBuffer contents]);

        float densitySum = 0.0f;
        float densityErrorSum = 0.0f;
        float maxObservedSpeed = 0.0f;
        for (std::size_t i = 0; i < particleCount; ++i) {
            densitySum += densities[i];
            densityErrorSum += std::abs(densities[i] - settings.restDensity) / settings.restDensity;
            maxObservedSpeed = std::max(maxObservedSpeed, simd_length(velocities[i].xyz));
        }

        const float count = static_cast<float>(std::max<std::size_t>(particleCount, 1));
        stats.averageDensity = densitySum / count;
        stats.averageDensityError = densityErrorSum / count;
        stats.maxSpeed = maxObservedSpeed;
    }
};

MetalSimulationBackend::MetalSimulationBackend()
    : impl_(new Impl()) {}

MetalSimulationBackend::~MetalSimulationBackend() {
    delete impl_;
}

bool MetalSimulationBackend::available() const {
    return impl_ != nullptr && impl_->available;
}

void MetalSimulationBackend::reset(const WaterSimulationSettings& settings) {
    if (!available()) {
        return;
    }

    impl_->settings = settings;
    sanitizeSettings(impl_->settings);
    impl_->updateDerivedConfig();
    impl_->allocateBuffers();

    auto* positions = static_cast<simd_float4*>([impl_->positionsBuffer contents]);
    auto* predicted = static_cast<simd_float4*>([impl_->predictedBuffer contents]);
    auto* velocities = static_cast<simd_float4*>([impl_->velocitiesBuffer contents]);
    auto* corrections = static_cast<simd_float4*>([impl_->correctionsBuffer contents]);
    auto* densities = static_cast<float*>([impl_->densitiesBuffer contents]);
    auto* lambdas = static_cast<float*>([impl_->lambdasBuffer contents]);
    auto* metrics = static_cast<simd_float4*>([impl_->metricsBuffer contents]);
    auto* interactionHeat = static_cast<float*>([impl_->interactionHeatBuffer contents]);
    auto* foam = static_cast<float*>([impl_->foamBuffer contents]);

    const float spacing = impl_->settings.particleRadius * kSpawnSpacingScale;
    const float startX = -0.5f * static_cast<float>(impl_->settings.particlesX - 1) * spacing;
    const float startZ = -0.5f * static_cast<float>(impl_->settings.particlesZ - 1) * spacing;
    const float startY = impl_->settings.containerFloorY + impl_->settings.particleRadius * 1.6f;

    std::size_t index = 0;
    for (int y = 0; y < impl_->settings.particlesY; ++y) {
        for (int z = 0; z < impl_->settings.particlesZ; ++z) {
            for (int x = 0; x < impl_->settings.particlesX; ++x) {
                const float jitterA = std::sin(static_cast<float>(index) * 1.6180339f);
                const float jitterB = std::cos(static_cast<float>(index) * 2.4142136f);
                glm::vec3 position(
                    startX + static_cast<float>(x) * spacing + jitterA * impl_->settings.particleRadius * 0.02f,
                    startY + static_cast<float>(y) * spacing + jitterB * impl_->settings.particleRadius * 0.015f,
                    startZ + static_cast<float>(z) * spacing + jitterB * impl_->settings.particleRadius * 0.02f
                );
                position.x = glm::clamp(
                    position.x,
                    -impl_->halfDomain + impl_->settings.particleRadius,
                    impl_->halfDomain - impl_->settings.particleRadius
                );
                position.y = glm::clamp(
                    position.y,
                    impl_->settings.containerFloorY + impl_->settings.particleRadius,
                    impl_->settings.containerLipY - impl_->settings.particleRadius
                );
                position.z = glm::clamp(
                    position.z,
                    -impl_->halfDomain + impl_->settings.particleRadius,
                    impl_->halfDomain - impl_->settings.particleRadius
                );

                positions[index] = simd_make_float4(position.x, position.y, position.z, 1.0f);
                predicted[index] = positions[index];
                velocities[index] = simd_make_float4(0.0f, 0.0f, 0.0f, 0.0f);
                corrections[index] = simd_make_float4(0.0f, 0.0f, 0.0f, 0.0f);
                densities[index] = impl_->settings.restDensity;
                lambdas[index] = 0.0f;
                metrics[index] = simd_make_float4(1.0f, 0.0f, 0.0f, 0.0f);
                interactionHeat[index] = 0.0f;
                foam[index] = 0.0f;
                ++index;
            }
        }
    }

    impl_->stats = {};
    impl_->stats.particleCount = impl_->particleCount;
    impl_->updateDerivedStateFromPositions();
    impl_->rebuildGrid(1.0f / 120.0f);
}

void MetalSimulationBackend::updateSettings(const WaterSimulationSettings& settings, bool structuralReset) {
    if (!available()) {
        return;
    }

    WaterSimulationSettings nextSettings = settings;
    sanitizeSettings(nextSettings);
    const bool requiresReset =
        structuralReset ||
        settings.backend != impl_->settings.backend ||
        structuralDifference(nextSettings, impl_->settings) > 1.0e-5f;
    if (requiresReset) {
        reset(nextSettings);
        return;
    }

    impl_->settings = nextSettings;
}

void MetalSimulationBackend::step(float deltaTime) {
    if (!available()) {
        return;
    }

    const auto totalStart = Clock::now();
    impl_->stats.frameIndex += 1;
    impl_->stats.particleCount = impl_->particleCount;
    impl_->stats.gridBuildMs = 0.0f;
    impl_->stats.densityPassMs = 0.0f;
    impl_->stats.constraintPassMs = 0.0f;
    impl_->stats.integrateMs = 0.0f;
    impl_->stats.finalizeMs = 0.0f;
    impl_->stats.snapshotMs = 0.0f;
    impl_->stats.totalStepMs = 0.0f;
    impl_->stats.neighborSamples = 0;
    impl_->stats.simulatedDeltaTime = glm::clamp(deltaTime, kMinDeltaTime, kMaxDeltaTime);

    const float clampedDeltaTime = impl_->stats.simulatedDeltaTime;
    const float safeTravel =
        std::max(impl_->settings.cflFactor * impl_->settings.smoothingLength, impl_->settings.particleRadius * 0.5f);
    const float travelEstimate =
        impl_->stats.maxSpeed * clampedDeltaTime + impl_->settings.gravity * clampedDeltaTime * clampedDeltaTime;
    const int cflSubsteps = std::max(1, static_cast<int>(std::ceil(travelEstimate / std::max(safeTravel, kEpsilon))));
    const int effectiveSubsteps = std::clamp(
        std::max(impl_->settings.substeps, cflSubsteps),
        1,
        std::max(impl_->settings.maxSubsteps, impl_->settings.substeps)
    );
    impl_->stats.executedSubsteps = effectiveSubsteps;
    impl_->stats.executedSolverIterations = impl_->settings.pressureIterations;
    const float substepDelta = clampedDeltaTime / static_cast<float>(effectiveSubsteps);

    for (int substep = 0; substep < effectiveSubsteps; ++substep) {
        auto phaseStart = Clock::now();
        impl_->runIntegrate(substepDelta);
        impl_->stats.integrateMs += toMilliseconds(Clock::now() - phaseStart);

        for (int iteration = 0; iteration < impl_->settings.pressureIterations; ++iteration) {
            phaseStart = Clock::now();
            impl_->rebuildGrid(substepDelta);
            impl_->stats.gridBuildMs += toMilliseconds(Clock::now() - phaseStart);

            phaseStart = Clock::now();
            impl_->runDensityLambda(substepDelta);
            impl_->stats.densityPassMs += toMilliseconds(Clock::now() - phaseStart);

            phaseStart = Clock::now();
            impl_->runCorrections(substepDelta);
            impl_->stats.constraintPassMs += toMilliseconds(Clock::now() - phaseStart);
        }

        phaseStart = Clock::now();
        impl_->rebuildGrid(substepDelta);
        impl_->stats.gridBuildMs += toMilliseconds(Clock::now() - phaseStart);

        phaseStart = Clock::now();
        impl_->runFinalize(substepDelta);
        impl_->stats.finalizeMs += toMilliseconds(Clock::now() - phaseStart);
    }

    impl_->updateDerivedStateFromPositions();
    impl_->reduceStats();
    impl_->stats.totalStepMs = toMilliseconds(Clock::now() - totalStart);
}

void MetalSimulationBackend::addImpulse(
    const glm::vec2& worldXZ,
    const glm::vec2& dragVelocity,
    float heightDelta,
    float radius
) {
    if (!available()) {
        return;
    }

    if (std::abs(worldXZ.x) > impl_->halfDomain || std::abs(worldXZ.y) > impl_->halfDomain) {
        return;
    }

    glm::vec2 clampedDrag = dragVelocity;
    const float dragMagnitude = glm::length(clampedDrag);
    if (dragMagnitude > impl_->settings.interactionMaxSpeed && dragMagnitude > kEpsilon) {
        clampedDrag *= impl_->settings.interactionMaxSpeed / dragMagnitude;
    }

    const float clampedRadius = std::max(radius, impl_->settings.particleRadius * 2.5f);
    const float radiusSquared = clampedRadius * clampedRadius;
    const float verticalImpulse =
        glm::clamp(-heightDelta * impl_->settings.interactionLift * 52.0f, -1.15f, 1.15f);
    const glm::vec3 directionalImpulse(
        clampedDrag.x * impl_->settings.interactionForce,
        verticalImpulse,
        clampedDrag.y * impl_->settings.interactionForce
    );

    const glm::vec3 center(worldXZ.x, impl_->interactionPlaneY, worldXZ.y);
    const glm::vec3 minPoint = center - glm::vec3(clampedRadius);
    const glm::vec3 maxPoint = center + glm::vec3(clampedRadius);

    auto positionToCell = [&](const glm::vec3& position) {
        const glm::vec3 gridMin(-impl_->halfDomain, impl_->settings.containerFloorY, -impl_->halfDomain);
        const glm::vec3 normalized = (position - gridMin) / impl_->settings.smoothingLength;
        return glm::ivec3(
            glm::clamp(static_cast<int>(std::floor(normalized.x)), 0, impl_->gridResolutionX - 1),
            glm::clamp(static_cast<int>(std::floor(normalized.y)), 0, impl_->gridResolutionY - 1),
            glm::clamp(static_cast<int>(std::floor(normalized.z)), 0, impl_->gridResolutionZ - 1)
        );
    };

    const glm::ivec3 minCell = positionToCell(minPoint);
    const glm::ivec3 maxCell = positionToCell(maxPoint);
    auto* positions = static_cast<simd_float4*>([impl_->positionsBuffer contents]);
    auto* velocities = static_cast<simd_float4*>([impl_->velocitiesBuffer contents]);
    auto* interactionHeat = static_cast<float*>([impl_->interactionHeatBuffer contents]);
    auto* foam = static_cast<float*>([impl_->foamBuffer contents]);
    const auto* cellStarts = static_cast<const std::uint32_t*>([impl_->cellStartsBuffer contents]);
    const auto* sortedIndices = static_cast<const std::uint32_t*>([impl_->sortedParticleIndicesBuffer contents]);

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                const std::size_t cellIndex =
                    (static_cast<std::size_t>(z) * static_cast<std::size_t>(impl_->gridResolutionY) + static_cast<std::size_t>(y)) *
                        static_cast<std::size_t>(impl_->gridResolutionX) +
                    static_cast<std::size_t>(x);
                for (std::uint32_t entry = cellStarts[cellIndex]; entry < cellStarts[cellIndex + 1]; ++entry) {
                    const std::size_t particleIndex = static_cast<std::size_t>(sortedIndices[entry]);
                    const glm::vec3 offset(
                        positions[particleIndex].x - center.x,
                        positions[particleIndex].y - center.y,
                        positions[particleIndex].z - center.z
                    );
                    const float distanceSquared = glm::dot(offset, offset);
                    if (distanceSquared > radiusSquared) {
                        continue;
                    }

                    const float distance = std::sqrt(std::max(distanceSquared, 0.0f));
                    const float q = 1.0f - distance / clampedRadius;
                    const float influence = q * q * (3.0f - 2.0f * q);
                    const glm::vec3 swirl =
                        glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), offset) * (0.055f * influence);

                    glm::vec3 velocity(velocities[particleIndex].x, velocities[particleIndex].y, velocities[particleIndex].z);
                    velocity += (directionalImpulse + swirl) * influence;

                    const float speed = glm::length(velocity);
                    if (speed > impl_->settings.maxSpeed && speed > kEpsilon) {
                        velocity *= impl_->settings.maxSpeed / speed;
                    }

                    velocities[particleIndex] = simd_make_float4(velocity.x, velocity.y, velocity.z, 0.0f);
                    interactionHeat[particleIndex] = std::max(interactionHeat[particleIndex], influence * 0.85f);
                    foam[particleIndex] = std::max(foam[particleIndex], influence * 0.55f);
                }
            }
        }
    }
}

void MetalSimulationBackend::buildRenderSnapshot(WaterRenderSnapshot& snapshot) const {
    if (!available()) {
        return;
    }

    snapshot.particles.resize(impl_->particleCount);
    const auto* positions = static_cast<const simd_float4*>([impl_->positionsBuffer contents]);
    const auto* metrics = static_cast<const simd_float4*>([impl_->metricsBuffer contents]);
    for (std::size_t i = 0; i < impl_->particleCount; ++i) {
        snapshot.particles[i].position = glm::vec3(positions[i].x, positions[i].y, positions[i].z);
        snapshot.particles[i].metrics = glm::vec4(metrics[i].x, metrics[i].y, metrics[i].z, metrics[i].w);
    }

    snapshot.particleRadius = impl_->settings.particleRadius;
    snapshot.interactionPlaneY = impl_->interactionPlaneY;
    snapshot.halfDomainSize = impl_->halfDomain;
    snapshot.containerFloorY = impl_->settings.containerFloorY;
    snapshot.containerLipY = impl_->settings.containerLipY;
    snapshot.version = impl_->stats.frameIndex;
}

const SimulationStats& MetalSimulationBackend::stats() const {
    return impl_->stats;
}

float MetalSimulationBackend::interactionPlaneY() const {
    return impl_->interactionPlaneY;
}

std::size_t MetalSimulationBackend::particleCount() const {
    return impl_->particleCount;
}
