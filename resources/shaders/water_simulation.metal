#include <metal_stdlib>
using namespace metal;

constant float kPi = 3.14159265358979323846f;
constant float kEpsilon = 1.0e-6f;
constant float kQuietSpeedThreshold = 0.45f;
constant float kQuietDensityErrorThreshold = 0.08f;
constant uint kScanBlockSize = 256u;

struct MetalSimParams {
    uint particleCount;
    uint gridResolutionX;
    uint gridResolutionY;
    uint gridResolutionZ;
    float deltaTime;
    float smoothingLength;
    float smoothingLengthSquared;
    float particleMass;
    float restDensity;
    float pressureStiffness;
    float nearPressureStiffness;
    float viscosityLinear;
    float viscosityQuadratic;
    float xsphViscosity;
    float velocityDamping;
    float restVelocityDamping;
    float velocityTransfer;
    float positionRelaxation;
    float tensileCorrection;
    float boundaryDamping;
    float boundaryRestitution;
    float boundaryFriction;
    float poly6Coefficient;
    float spikyGradientCoefficient;
    float correctionReferenceKernel;
    float gravity;
    float maxSpeed;
    float maxSpeedSquared;
    float halfDomain;
    float containerFloorY;
    float containerLipY;
    float particleRadius;
    float interactionPlaneY;
    float foamDecay;
};

struct ScanParams {
    uint count;
};

inline float poly6Kernel(float distanceSquared, constant MetalSimParams& params) {
    if (distanceSquared >= params.smoothingLengthSquared) {
        return 0.0f;
    }

    float h2MinusR2 = params.smoothingLengthSquared - distanceSquared;
    return params.poly6Coefficient * h2MinusR2 * h2MinusR2 * h2MinusR2;
}

inline float3 spikyGradient(float3 delta, float distance, constant MetalSimParams& params) {
    if (distance <= kEpsilon || distance >= params.smoothingLength) {
        return float3(0.0f);
    }

    float scale =
        params.spikyGradientCoefficient *
        (params.smoothingLength - distance) *
        (params.smoothingLength - distance) /
        distance;
    return delta * scale;
}

inline uint positionToCellIndex(float3 position, constant MetalSimParams& params) {
    float3 gridMin = float3(-params.halfDomain, params.containerFloorY, -params.halfDomain);
    float3 normalized = (position - gridMin) / params.smoothingLength;
    uint cellX = uint(clamp(int(floor(normalized.x)), 0, int(params.gridResolutionX) - 1));
    uint cellY = uint(clamp(int(floor(normalized.y)), 0, int(params.gridResolutionY) - 1));
    uint cellZ = uint(clamp(int(floor(normalized.z)), 0, int(params.gridResolutionZ) - 1));
    return (cellZ * params.gridResolutionY + cellY) * params.gridResolutionX + cellX;
}

inline void applyBounds(thread float3& position, constant MetalSimParams& params) {
    position.x = clamp(position.x, -params.halfDomain + params.particleRadius, params.halfDomain - params.particleRadius);
    position.y = max(position.y, params.containerFloorY + params.particleRadius);
    position.z = clamp(position.z, -params.halfDomain + params.particleRadius, params.halfDomain - params.particleRadius);
}

kernel void integratePredictedKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* positions [[buffer(1)]],
    device float4* predictedPositions [[buffer(2)]],
    device float4* velocities [[buffer(3)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float damping = exp(-params.velocityDamping * params.deltaTime);
    float3 velocity = velocities[gid].xyz;
    velocity.y -= params.gravity * params.deltaTime;
    velocity *= damping;

    float speedSquared = dot(velocity, velocity);
    if (speedSquared > params.maxSpeedSquared) {
        velocity *= params.maxSpeed / sqrt(max(speedSquared, kEpsilon));
    }

    float3 predicted = positions[gid].xyz + velocity * params.deltaTime;
    applyBounds(predicted, params);

    velocities[gid] = float4(velocity, 0.0f);
    predictedPositions[gid] = float4(predicted, 1.0f);
}

kernel void assignCellsKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* predictedPositions [[buffer(1)]],
    device uint* particleCells [[buffer(2)]],
    device atomic_uint* cellCounts [[buffer(3)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    uint cellIndex = positionToCellIndex(predictedPositions[gid].xyz, params);
    particleCells[gid] = cellIndex;
    atomic_fetch_add_explicit(&cellCounts[cellIndex], 1u, memory_order_relaxed);
}

kernel void scanExclusiveBlocksKernel(
    constant ScanParams& scanParams [[buffer(0)]],
    device const uint* input [[buffer(1)]],
    device uint* output [[buffer(2)]],
    device uint* blockSums [[buffer(3)]],
    uint tid [[thread_index_in_threadgroup]],
    uint3 groupPosition [[threadgroup_position_in_grid]]
) {
    threadgroup uint shared[kScanBlockSize];

    uint globalIndex = groupPosition.x * kScanBlockSize + tid;
    shared[tid] = globalIndex < scanParams.count ? input[globalIndex] : 0u;
    threadgroup_barrier(mem_flags::mem_threadgroup);

    for (uint offset = 1u; offset < kScanBlockSize; offset <<= 1u) {
        uint prior = tid >= offset ? shared[tid - offset] : 0u;
        threadgroup_barrier(mem_flags::mem_threadgroup);
        shared[tid] += prior;
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }

    if (globalIndex < scanParams.count) {
        output[globalIndex] = tid == 0u ? 0u : shared[tid - 1u];
    }
    if (tid == kScanBlockSize - 1u) {
        blockSums[groupPosition.x] = shared[tid];
    }
}

kernel void addScanOffsetsKernel(
    constant ScanParams& scanParams [[buffer(0)]],
    device uint* values [[buffer(1)]],
    device const uint* blockOffsets [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= scanParams.count) {
        return;
    }

    uint blockIndex = gid / kScanBlockSize;
    if (blockIndex > 0u) {
        values[gid] += blockOffsets[blockIndex];
    }
}

kernel void writeTerminalCellStartKernel(
    constant ScanParams& scanParams [[buffer(0)]],
    device uint* cellStarts [[buffer(1)]],
    device const uint* cellCounts [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid > 0u || scanParams.count == 0u) {
        return;
    }

    cellStarts[scanParams.count] =
        cellStarts[scanParams.count - 1u] + cellCounts[scanParams.count - 1u];
}

kernel void scatterSortedKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const uint* particleCells [[buffer(1)]],
    device atomic_uint* cellWriteHeads [[buffer(2)]],
    device uint* sortedParticleIndices [[buffer(3)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    uint cellIndex = particleCells[gid];
    uint slot = atomic_fetch_add_explicit(&cellWriteHeads[cellIndex], 1u, memory_order_relaxed);
    sortedParticleIndices[slot] = gid;
}

kernel void computeDensityAlphaKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* predictedPositions [[buffer(1)]],
    device const uint* cellStarts [[buffer(2)]],
    device const uint* sortedParticleIndices [[buffer(3)]],
    device float* densities [[buffer(4)]],
    device float* alphas [[buffer(5)]],
    device float* densityErrors [[buffer(6)]],
    device atomic_uint* neighborCounter [[buffer(7)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float3 pi = predictedPositions[gid].xyz;
    uint baseCell = positionToCellIndex(pi, params);
    uint baseX = baseCell % params.gridResolutionX;
    uint baseY = (baseCell / params.gridResolutionX) % params.gridResolutionY;
    uint baseZ = baseCell / (params.gridResolutionX * params.gridResolutionY);

    float w0 = poly6Kernel(0.0f, params);
    float density = params.particleMass * w0;
    float3 gradI = float3(0.0f);
    float sumGradients = 0.0f;
    uint localNeighbors = 0u;

    for (int z = int(baseZ) - 1; z <= int(baseZ) + 1; ++z) {
        if (z < 0 || z >= int(params.gridResolutionZ)) {
            continue;
        }
        for (int y = int(baseY) - 1; y <= int(baseY) + 1; ++y) {
            if (y < 0 || y >= int(params.gridResolutionY)) {
                continue;
            }
            for (int x = int(baseX) - 1; x <= int(baseX) + 1; ++x) {
                if (x < 0 || x >= int(params.gridResolutionX)) {
                    continue;
                }

                uint cellIndex = (uint(z) * params.gridResolutionY + uint(y)) * params.gridResolutionX + uint(x);
                for (uint entry = cellStarts[cellIndex]; entry < cellStarts[cellIndex + 1]; ++entry) {
                    uint neighbor = sortedParticleIndices[entry];
                    if (neighbor == gid) {
                        continue;
                    }

                    float3 delta = pi - predictedPositions[neighbor].xyz;
                    float distanceSquared = dot(delta, delta);
                    if (distanceSquared >= params.smoothingLengthSquared) {
                        continue;
                    }

                    density += params.particleMass * poly6Kernel(distanceSquared, params);
                    float distance = sqrt(max(distanceSquared, kEpsilon));
                    float3 gradient =
                        (params.particleMass / max(params.restDensity, kEpsilon)) *
                        spikyGradient(delta, distance, params);
                    sumGradients += dot(gradient, gradient);
                    gradI += gradient;
                    localNeighbors += 1u;
                }
            }
        }
    }

    sumGradients += dot(gradI, gradI);
    densities[gid] = density;
    alphas[gid] = 1.0f / (sumGradients + 4.0e-3f);
    densityErrors[gid] = density / max(params.restDensity, kEpsilon) - 1.0f;
    atomic_fetch_add_explicit(neighborCounter, localNeighbors, memory_order_relaxed);
}

kernel void updateDensityPressureKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float* alphas [[buffer(1)]],
    device const float* densityErrors [[buffer(2)]],
    device float* densityPressures [[buffer(3)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float pressureSource = clamp(densityErrors[gid], -0.18f, 2.0f);
    densityPressures[gid] += -pressureSource * alphas[gid];
}

kernel void computeDensityCorrectionsKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* predictedPositions [[buffer(1)]],
    device const uint* cellStarts [[buffer(2)]],
    device const uint* sortedParticleIndices [[buffer(3)]],
    device const float* densityPressures [[buffer(4)]],
    device float4* corrections [[buffer(5)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float3 pi = predictedPositions[gid].xyz;
    uint baseCell = positionToCellIndex(pi, params);
    uint baseX = baseCell % params.gridResolutionX;
    uint baseY = (baseCell / params.gridResolutionX) % params.gridResolutionY;
    uint baseZ = baseCell / (params.gridResolutionX * params.gridResolutionY);

    float targetSeparation = params.particleRadius * 1.45f;
    float correctionScale =
        params.positionRelaxation * params.pressureStiffness / max(params.restDensity, kEpsilon);
    float tensileScale = params.tensileCorrection * params.nearPressureStiffness;

    float3 correction = float3(0.0f);

    for (int z = int(baseZ) - 1; z <= int(baseZ) + 1; ++z) {
        if (z < 0 || z >= int(params.gridResolutionZ)) {
            continue;
        }
        for (int y = int(baseY) - 1; y <= int(baseY) + 1; ++y) {
            if (y < 0 || y >= int(params.gridResolutionY)) {
                continue;
            }
            for (int x = int(baseX) - 1; x <= int(baseX) + 1; ++x) {
                if (x < 0 || x >= int(params.gridResolutionX)) {
                    continue;
                }

                uint cellIndex = (uint(z) * params.gridResolutionY + uint(y)) * params.gridResolutionX + uint(x);
                for (uint entry = cellStarts[cellIndex]; entry < cellStarts[cellIndex + 1]; ++entry) {
                    uint neighbor = sortedParticleIndices[entry];
                    if (neighbor == gid) {
                        continue;
                    }

                    float3 delta = pi - predictedPositions[neighbor].xyz;
                    float distanceSquared = dot(delta, delta);
                    if (distanceSquared >= params.smoothingLengthSquared) {
                        continue;
                    }

                    float distance = sqrt(max(distanceSquared, kEpsilon));
                    float3 gradient = spikyGradient(delta, distance, params);
                    float corr =
                        -tensileScale *
                        pow(poly6Kernel(distanceSquared, params) / params.correctionReferenceKernel, 4.0f);
                    correction += (densityPressures[gid] + densityPressures[neighbor] + corr) * gradient;

                    if (distance < targetSeparation) {
                        float overlap = targetSeparation - distance;
                        correction += (delta / distance) * (overlap * 0.014f);
                    }
                }
            }
        }
    }

    correction *= correctionScale;
    float correctionLength = length(correction);
    float maxCorrection = params.particleRadius * 0.2f;
    if (correctionLength > maxCorrection && correctionLength > kEpsilon) {
        correction *= maxCorrection / correctionLength;
    }

    corrections[gid] = float4(correction, 0.0f);
}

kernel void updateProvisionalVelocitiesKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* positions [[buffer(1)]],
    device const float4* predictedPositions [[buffer(2)]],
    device float4* provisionalVelocities [[buffer(3)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float dt = max(params.deltaTime, kEpsilon);
    provisionalVelocities[gid] = float4((predictedPositions[gid].xyz - positions[gid].xyz) / dt, 0.0f);
}

kernel void computeDivergenceAlphaKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* predictedPositions [[buffer(1)]],
    device const uint* cellStarts [[buffer(2)]],
    device const uint* sortedParticleIndices [[buffer(3)]],
    device const float4* provisionalVelocities [[buffer(4)]],
    device float* alphas [[buffer(5)]],
    device float* divergenceErrors [[buffer(6)]],
    device atomic_uint* neighborCounter [[buffer(7)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float3 pi = predictedPositions[gid].xyz;
    float3 velocityI = provisionalVelocities[gid].xyz;
    uint baseCell = positionToCellIndex(pi, params);
    uint baseX = baseCell % params.gridResolutionX;
    uint baseY = (baseCell / params.gridResolutionX) % params.gridResolutionY;
    uint baseZ = baseCell / (params.gridResolutionX * params.gridResolutionY);

    float3 gradI = float3(0.0f);
    float sumGradients = 0.0f;
    float divergence = 0.0f;
    uint localNeighbors = 0u;

    for (int z = int(baseZ) - 1; z <= int(baseZ) + 1; ++z) {
        if (z < 0 || z >= int(params.gridResolutionZ)) {
            continue;
        }
        for (int y = int(baseY) - 1; y <= int(baseY) + 1; ++y) {
            if (y < 0 || y >= int(params.gridResolutionY)) {
                continue;
            }
            for (int x = int(baseX) - 1; x <= int(baseX) + 1; ++x) {
                if (x < 0 || x >= int(params.gridResolutionX)) {
                    continue;
                }

                uint cellIndex = (uint(z) * params.gridResolutionY + uint(y)) * params.gridResolutionX + uint(x);
                for (uint entry = cellStarts[cellIndex]; entry < cellStarts[cellIndex + 1]; ++entry) {
                    uint neighbor = sortedParticleIndices[entry];
                    if (neighbor == gid) {
                        continue;
                    }

                    float3 delta = pi - predictedPositions[neighbor].xyz;
                    float distanceSquared = dot(delta, delta);
                    if (distanceSquared >= params.smoothingLengthSquared) {
                        continue;
                    }

                    float distance = sqrt(max(distanceSquared, kEpsilon));
                    float3 gradient =
                        (params.particleMass / max(params.restDensity, kEpsilon)) *
                        spikyGradient(delta, distance, params);
                    sumGradients += dot(gradient, gradient);
                    gradI += gradient;
                    divergence += dot(velocityI - provisionalVelocities[neighbor].xyz, gradient);
                    localNeighbors += 1u;
                }
            }
        }
    }

    sumGradients += dot(gradI, gradI);
    alphas[gid] = 1.0f / (sumGradients + 4.0e-3f);
    divergenceErrors[gid] = clamp(divergence * params.deltaTime, 0.0f, 2.0f);
    atomic_fetch_add_explicit(neighborCounter, localNeighbors, memory_order_relaxed);
}

kernel void updateDivergencePressureKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float* alphas [[buffer(1)]],
    device const float* divergenceErrors [[buffer(2)]],
    device float* divergencePressures [[buffer(3)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    divergencePressures[gid] += -divergenceErrors[gid] * alphas[gid];
}

kernel void solveDivergenceKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* predictedPositions [[buffer(1)]],
    device const uint* cellStarts [[buffer(2)]],
    device const uint* sortedParticleIndices [[buffer(3)]],
    device const float* divergencePressures [[buffer(4)]],
    device float4* provisionalVelocities [[buffer(5)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float3 pi = predictedPositions[gid].xyz;
    uint baseCell = positionToCellIndex(pi, params);
    uint baseX = baseCell % params.gridResolutionX;
    uint baseY = (baseCell / params.gridResolutionX) % params.gridResolutionY;
    uint baseZ = baseCell / (params.gridResolutionX * params.gridResolutionY);

    float3 velocityCorrection = float3(0.0f);

    for (int z = int(baseZ) - 1; z <= int(baseZ) + 1; ++z) {
        if (z < 0 || z >= int(params.gridResolutionZ)) {
            continue;
        }
        for (int y = int(baseY) - 1; y <= int(baseY) + 1; ++y) {
            if (y < 0 || y >= int(params.gridResolutionY)) {
                continue;
            }
            for (int x = int(baseX) - 1; x <= int(baseX) + 1; ++x) {
                if (x < 0 || x >= int(params.gridResolutionX)) {
                    continue;
                }

                uint cellIndex = (uint(z) * params.gridResolutionY + uint(y)) * params.gridResolutionX + uint(x);
                for (uint entry = cellStarts[cellIndex]; entry < cellStarts[cellIndex + 1]; ++entry) {
                    uint neighbor = sortedParticleIndices[entry];
                    if (neighbor == gid) {
                        continue;
                    }

                    float3 delta = pi - predictedPositions[neighbor].xyz;
                    float distanceSquared = dot(delta, delta);
                    if (distanceSquared >= params.smoothingLengthSquared) {
                        continue;
                    }

                    float distance = sqrt(max(distanceSquared, kEpsilon));
                    float3 gradient = spikyGradient(delta, distance, params);
                    velocityCorrection +=
                        (divergencePressures[gid] + divergencePressures[neighbor]) *
                        params.particleMass *
                        gradient;
                }
            }
        }
    }

    float3 updatedVelocity = provisionalVelocities[gid].xyz + velocityCorrection * params.deltaTime;
    float speedSquared = dot(updatedVelocity, updatedVelocity);
    if (speedSquared > params.maxSpeedSquared) {
        updatedVelocity *= params.maxSpeed / sqrt(max(speedSquared, kEpsilon));
    }
    provisionalVelocities[gid] = float4(updatedVelocity, 0.0f);
}

kernel void applyCorrectionsKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device float4* predictedPositions [[buffer(1)]],
    device const float4* corrections [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float3 predicted = predictedPositions[gid].xyz + corrections[gid].xyz;
    applyBounds(predicted, params);
    predictedPositions[gid] = float4(predicted, 1.0f);
}

kernel void finalizeKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device float4* positions [[buffer(1)]],
    device const float4* predictedPositions [[buffer(2)]],
    device float4* velocities [[buffer(3)]],
    device const float4* provisionalVelocities [[buffer(4)]],
    device const uint* cellStarts [[buffer(5)]],
    device const uint* sortedParticleIndices [[buffer(6)]],
    device float* densities [[buffer(7)]],
    device const float* divergenceErrors [[buffer(8)]],
    device float4* metrics [[buffer(9)]],
    device float* interactionHeat [[buffer(10)]],
    device float* foam [[buffer(11)]],
    device atomic_uint* neighborCounter [[buffer(12)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float3 predicted = predictedPositions[gid].xyz;
    float3 previousVelocity = velocities[gid].xyz;
    float3 baseVelocity = provisionalVelocities[gid].xyz;
    uint baseCell = positionToCellIndex(predicted, params);
    uint baseX = baseCell % params.gridResolutionX;
    uint baseY = (baseCell / params.gridResolutionX) % params.gridResolutionY;
    uint baseZ = baseCell / (params.gridResolutionX * params.gridResolutionY);

    float w0 = poly6Kernel(0.0f, params);
    float density = params.particleMass * w0;
    float3 xsph = float3(0.0f);
    float3 pairwiseViscosity = float3(0.0f);
    uint localNeighbors = 0u;

    for (int z = int(baseZ) - 1; z <= int(baseZ) + 1; ++z) {
        if (z < 0 || z >= int(params.gridResolutionZ)) {
            continue;
        }
        for (int y = int(baseY) - 1; y <= int(baseY) + 1; ++y) {
            if (y < 0 || y >= int(params.gridResolutionY)) {
                continue;
            }
            for (int x = int(baseX) - 1; x <= int(baseX) + 1; ++x) {
                if (x < 0 || x >= int(params.gridResolutionX)) {
                    continue;
                }

                uint cellIndex = (uint(z) * params.gridResolutionY + uint(y)) * params.gridResolutionX + uint(x);
                for (uint entry = cellStarts[cellIndex]; entry < cellStarts[cellIndex + 1]; ++entry) {
                    uint neighbor = sortedParticleIndices[entry];
                    if (neighbor == gid) {
                        continue;
                    }

                    float3 delta = predicted - predictedPositions[neighbor].xyz;
                    float distanceSquared = dot(delta, delta);
                    if (distanceSquared >= params.smoothingLengthSquared) {
                        continue;
                    }

                    float kernelWeight = poly6Kernel(distanceSquared, params);
                    density += params.particleMass * kernelWeight;
                    float3 neighborVelocity = provisionalVelocities[neighbor].xyz - provisionalVelocities[gid].xyz;
                    xsph += neighborVelocity * kernelWeight;

                    float distance = sqrt(max(distanceSquared, kEpsilon));
                    float q = clamp(1.0f - distance / params.smoothingLength, 0.0f, 1.0f);
                    if (q > 0.0f) {
                        float3 normalizedDirection = delta / distance;
                        float radialVelocity = dot(-neighborVelocity, normalizedDirection);
                        if (radialVelocity < 0.0f) {
                            float approachSpeed = -radialVelocity;
                            float viscosityImpulse =
                                params.deltaTime *
                                q *
                                (params.viscosityLinear * approachSpeed +
                                 params.viscosityQuadratic * approachSpeed * approachSpeed);
                            pairwiseViscosity += normalizedDirection * (0.5f * viscosityImpulse);
                        }
                    }
                    localNeighbors += 1u;
                }
            }
        }
    }

    densities[gid] = density;
    float densityRatio = density / max(params.restDensity, kEpsilon);
    float densityError = fabs(densityRatio - 1.0f);
    float3 relaxedVelocity =
        mix(previousVelocity, baseVelocity, params.velocityTransfer) +
        pairwiseViscosity +
        xsph * (params.xsphViscosity * params.particleMass / max(density, kEpsilon));

    float projectedSpeed = length(baseVelocity);
    float quietWeight =
        clamp(1.0f - projectedSpeed / kQuietSpeedThreshold, 0.0f, 1.0f) *
        clamp(1.0f - densityError / kQuietDensityErrorThreshold, 0.0f, 1.0f);
    relaxedVelocity *= exp(-params.restVelocityDamping * quietWeight * params.deltaTime);

    float tangentialScale = max(0.0f, 1.0f - params.boundaryFriction - params.boundaryDamping * 0.15f);
    float minX = -params.halfDomain + params.particleRadius;
    float maxX = params.halfDomain - params.particleRadius;
    float minY = params.containerFloorY + params.particleRadius;
    float minZ = -params.halfDomain + params.particleRadius;
    float maxZ = params.halfDomain - params.particleRadius;

    if (predicted.x <= minX + kEpsilon && relaxedVelocity.x < 0.0f) {
        relaxedVelocity.x = -relaxedVelocity.x * params.boundaryRestitution;
        relaxedVelocity.y *= tangentialScale;
        relaxedVelocity.z *= tangentialScale;
    } else if (predicted.x >= maxX - kEpsilon && relaxedVelocity.x > 0.0f) {
        relaxedVelocity.x = -relaxedVelocity.x * params.boundaryRestitution;
        relaxedVelocity.y *= tangentialScale;
        relaxedVelocity.z *= tangentialScale;
    }

    if (predicted.y <= minY + kEpsilon && relaxedVelocity.y < 0.0f) {
        relaxedVelocity.y = -relaxedVelocity.y * params.boundaryRestitution;
        relaxedVelocity.x *= tangentialScale;
        relaxedVelocity.z *= tangentialScale;
        if (fabs(relaxedVelocity.y) < 0.04f) {
            relaxedVelocity.y = 0.0f;
        }
    }

    if (predicted.z <= minZ + kEpsilon && relaxedVelocity.z < 0.0f) {
        relaxedVelocity.z = -relaxedVelocity.z * params.boundaryRestitution;
        relaxedVelocity.x *= tangentialScale;
        relaxedVelocity.y *= tangentialScale;
    } else if (predicted.z >= maxZ - kEpsilon && relaxedVelocity.z > 0.0f) {
        relaxedVelocity.z = -relaxedVelocity.z * params.boundaryRestitution;
        relaxedVelocity.x *= tangentialScale;
        relaxedVelocity.y *= tangentialScale;
    }

    relaxedVelocity *= exp(-params.boundaryDamping * quietWeight * params.deltaTime * 0.65f);

    float speed = length(relaxedVelocity);
    if (speed > params.maxSpeed && speed > kEpsilon) {
        relaxedVelocity *= params.maxSpeed / speed;
    }

    positions[gid] = float4(predicted, 1.0f);
    velocities[gid] = float4(relaxedVelocity, 0.0f);

    float speedMetric = clamp(speed / max(params.maxSpeed, kEpsilon), 0.0f, 1.0f);
    float churn = clamp((densityRatio - 0.95f) * 0.45f + speedMetric * 0.22f, 0.0f, 1.0f);
    float foamDamping = exp(-params.foamDecay * params.deltaTime);
    foam[gid] = max(foam[gid] * foamDamping, churn);
    interactionHeat[gid] *= exp(-params.foamDecay * params.deltaTime);
    float interactionMetric = clamp(max(interactionHeat[gid], foam[gid] * 0.45f), 0.0f, 1.0f);

    metrics[gid] = float4(
        clamp(densityRatio, 0.0f, 2.0f),
        speedMetric,
        clamp(densityError * 1.2f + fabs(divergenceErrors[gid]) * 0.8f + quietWeight * 0.05f, 0.0f, 1.0f),
        interactionMetric
    );

    atomic_fetch_add_explicit(neighborCounter, localNeighbors, memory_order_relaxed);
}
