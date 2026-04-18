#include <metal_stdlib>
using namespace metal;

constant float kPi = 3.14159265358979323846f;
constant float kEpsilon = 1.0e-6f;
constant float kQuietSpeedThreshold = 0.45f;
constant float kQuietDensityErrorThreshold = 0.08f;

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
    float xsphViscosity;
    float velocityDamping;
    float restVelocityDamping;
    float velocityTransfer;
    float positionRelaxation;
    float tensileCorrection;
    float boundaryDamping;
    float boundaryRestitution;
    float boundaryFriction;
    float gravity;
    float maxSpeed;
    float maxSpeedSquared;
    float halfDomain;
    float containerFloorY;
    float containerLipY;
    float particleRadius;
    float interactionPlaneY;
};

inline float poly6Kernel(float distanceSquared, float smoothingLength) {
    if (distanceSquared >= smoothingLength * smoothingLength) {
        return 0.0f;
    }

    float h2MinusR2 = smoothingLength * smoothingLength - distanceSquared;
    float coefficient = 315.0f / (64.0f * kPi * pow(smoothingLength, 9.0f));
    return coefficient * h2MinusR2 * h2MinusR2 * h2MinusR2;
}

inline float3 spikyGradient(float3 delta, float distance, float smoothingLength) {
    if (distance <= kEpsilon || distance >= smoothingLength) {
        return float3(0.0f);
    }

    float coefficient = -45.0f / (kPi * pow(smoothingLength, 6.0f));
    float scale = coefficient * (smoothingLength - distance) * (smoothingLength - distance) / distance;
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
    position.y = clamp(position.y, params.containerFloorY + params.particleRadius, params.containerLipY - params.particleRadius);
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

kernel void computeDensityLambdaKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* predictedPositions [[buffer(1)]],
    device const uint* cellStarts [[buffer(2)]],
    device const uint* sortedParticleIndices [[buffer(3)]],
    device float* densities [[buffer(4)]],
    device float* lambdas [[buffer(5)]],
    device atomic_uint* neighborCounter [[buffer(6)]],
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

    float w0 = poly6Kernel(0.0f, params.smoothingLength);
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

                    density += params.particleMass * poly6Kernel(distanceSquared, params.smoothingLength);
                    float distance = sqrt(max(distanceSquared, kEpsilon));
                    float3 gradient =
                        (params.particleMass / max(params.restDensity, kEpsilon)) *
                        spikyGradient(delta, distance, params.smoothingLength);
                    sumGradients += dot(gradient, gradient);
                    gradI += gradient;
                    localNeighbors += 1u;
                }
            }
        }
    }

    sumGradients += dot(gradI, gradI);
    float constraint = clamp(density / max(params.restDensity, kEpsilon) - 1.0f, -0.12f, 1.5f);
    densities[gid] = density;
    lambdas[gid] = -constraint / (sumGradients + 4.0e-3f);
    atomic_fetch_add_explicit(neighborCounter, localNeighbors, memory_order_relaxed);
}

kernel void computePositionCorrectionsKernel(
    constant MetalSimParams& params [[buffer(0)]],
    device const float4* predictedPositions [[buffer(1)]],
    device const uint* cellStarts [[buffer(2)]],
    device const uint* sortedParticleIndices [[buffer(3)]],
    device const float* lambdas [[buffer(4)]],
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

    float correctionReference = max(
        poly6Kernel(params.smoothingLengthSquared * 0.12f, params.smoothingLength),
        kEpsilon
    );
    float targetSeparation = params.particleRadius * 1.65f;
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
                    float3 gradient = spikyGradient(delta, distance, params.smoothingLength);
                    float corr =
                        -tensileScale *
                        pow(poly6Kernel(distanceSquared, params.smoothingLength) / correctionReference, 4.0f);
                    correction += (lambdas[gid] + lambdas[neighbor] + corr) * gradient;

                    if (distance < targetSeparation) {
                        float overlap = targetSeparation - distance;
                        correction += (delta / distance) * (overlap * 0.025f);
                    }
                }
            }
        }
    }

    correction *= correctionScale;
    float correctionLength = length(correction);
    float maxCorrection = params.particleRadius * 0.22f;
    if (correctionLength > maxCorrection && correctionLength > kEpsilon) {
        correction *= maxCorrection / correctionLength;
    }

    corrections[gid] = float4(correction, 0.0f);
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
    device const uint* cellStarts [[buffer(4)]],
    device const uint* sortedParticleIndices [[buffer(5)]],
    device float* densities [[buffer(6)]],
    device float4* metrics [[buffer(7)]],
    device float* interactionHeat [[buffer(8)]],
    device float* foam [[buffer(9)]],
    device atomic_uint* neighborCounter [[buffer(10)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= params.particleCount) {
        return;
    }

    float3 predicted = predictedPositions[gid].xyz;
    float3 previousVelocity = velocities[gid].xyz;
    uint baseCell = positionToCellIndex(predicted, params);
    uint baseX = baseCell % params.gridResolutionX;
    uint baseY = (baseCell / params.gridResolutionX) % params.gridResolutionY;
    uint baseZ = baseCell / (params.gridResolutionX * params.gridResolutionY);

    float w0 = poly6Kernel(0.0f, params.smoothingLength);
    float density = params.particleMass * w0;
    float3 xsph = float3(0.0f);
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

                    float kernel = poly6Kernel(distanceSquared, params.smoothingLength);
                    density += params.particleMass * kernel;
                    xsph += (velocities[neighbor].xyz - previousVelocity) * kernel;
                    localNeighbors += 1u;
                }
            }
        }
    }

    densities[gid] = density;
    float densityRatio = density / max(params.restDensity, kEpsilon);
    float densityError = fabs(densityRatio - 1.0f);

    float3 projectedVelocity = (predicted - positions[gid].xyz) / max(params.deltaTime, kEpsilon);
    float3 relaxedVelocity =
        previousVelocity +
        xsph * (params.xsphViscosity * params.particleMass / max(density, kEpsilon));
    float3 newVelocity = mix(relaxedVelocity, projectedVelocity, params.velocityTransfer);

    float projectedSpeed = length(projectedVelocity);
    float quietWeight =
        clamp(1.0f - projectedSpeed / kQuietSpeedThreshold, 0.0f, 1.0f) *
        clamp(1.0f - densityError / kQuietDensityErrorThreshold, 0.0f, 1.0f);
    newVelocity *= exp(-params.restVelocityDamping * quietWeight * params.deltaTime);

    float tangentialScale = max(0.0f, 1.0f - params.boundaryFriction - params.boundaryDamping * 0.15f);
    float minX = -params.halfDomain + params.particleRadius;
    float maxX = params.halfDomain - params.particleRadius;
    float minY = params.containerFloorY + params.particleRadius;
    float maxY = params.containerLipY - params.particleRadius;
    float minZ = -params.halfDomain + params.particleRadius;
    float maxZ = params.halfDomain - params.particleRadius;

    if (predicted.x <= minX + kEpsilon && newVelocity.x < 0.0f) {
        newVelocity.x = -newVelocity.x * params.boundaryRestitution;
        newVelocity.y *= tangentialScale;
        newVelocity.z *= tangentialScale;
    } else if (predicted.x >= maxX - kEpsilon && newVelocity.x > 0.0f) {
        newVelocity.x = -newVelocity.x * params.boundaryRestitution;
        newVelocity.y *= tangentialScale;
        newVelocity.z *= tangentialScale;
    }

    if (predicted.y <= minY + kEpsilon && newVelocity.y < 0.0f) {
        newVelocity.y = -newVelocity.y * params.boundaryRestitution;
        newVelocity.x *= tangentialScale;
        newVelocity.z *= tangentialScale;
        if (fabs(newVelocity.y) < 0.04f) {
            newVelocity.y = 0.0f;
        }
    } else if (predicted.y >= maxY - kEpsilon && newVelocity.y > 0.0f) {
        newVelocity.y = -newVelocity.y * params.boundaryRestitution;
        newVelocity.x *= tangentialScale;
        newVelocity.z *= tangentialScale;
    }

    if (predicted.z <= minZ + kEpsilon && newVelocity.z < 0.0f) {
        newVelocity.z = -newVelocity.z * params.boundaryRestitution;
        newVelocity.x *= tangentialScale;
        newVelocity.y *= tangentialScale;
    } else if (predicted.z >= maxZ - kEpsilon && newVelocity.z > 0.0f) {
        newVelocity.z = -newVelocity.z * params.boundaryRestitution;
        newVelocity.x *= tangentialScale;
        newVelocity.y *= tangentialScale;
    }

    newVelocity *= exp(-params.boundaryDamping * quietWeight * params.deltaTime * 0.65f);

    float speed = length(newVelocity);
    if (speed > params.maxSpeed && speed > kEpsilon) {
        newVelocity *= params.maxSpeed / speed;
    }

    positions[gid] = float4(predicted, 1.0f);
    velocities[gid] = float4(newVelocity, 0.0f);

    float speedMetric = clamp(speed / max(params.maxSpeed, kEpsilon), 0.0f, 1.0f);
    float churn = clamp((densityRatio - 0.95f) * 0.45f + speedMetric * 0.22f, 0.0f, 1.0f);
    float foamDamping = exp(-1.8f * params.deltaTime);
    foam[gid] = max(foam[gid] * foamDamping, churn);
    interactionHeat[gid] *= exp(-1.8f * params.deltaTime);
    float interactionMetric = clamp(max(interactionHeat[gid], foam[gid] * 0.45f), 0.0f, 1.0f);

    metrics[gid] = float4(
        clamp(densityRatio, 0.0f, 2.0f),
        speedMetric,
        clamp(densityError * 1.5f + quietWeight * 0.05f, 0.0f, 1.0f),
        interactionMetric
    );

    atomic_fetch_add_explicit(neighborCounter, localNeighbors, memory_order_relaxed);
}
