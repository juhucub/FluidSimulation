# Implementation Notes

## Core Modules

- `include/sim/WaterSimulation.h`
  Public simulation settings, stats, snapshot structures, and the backend-facing simulation class.
- `src/sim/WaterSimulation.cpp`
  CPU backend implementation, backend switching, solver passes, impulse injection, and snapshot generation.
- `include/platform/MetalSimulationBackend.h`
  Bridge interface for the Metal simulation path.
- `src/platform/MetalSimulationBackend.mm`
  Metal device setup, pipeline management, shared buffers, GPU execution, and snapshot extraction.
- `resources/shaders/water_simulation.metal`
  Metal compute kernels for the GPU solver path.
- `include/render/WaterRenderer.h` and `src/render/WaterRenderer.cpp`
  OpenGL rendering path for compact particle snapshots and basin geometry.
- `src/app/main.cpp`
  Window setup, worker-thread orchestration, UI, runtime controls, and benchmark entrypoints.

## CPU Backend

The CPU implementation is structured around:

- SoA particle arrays for positions, predicted positions, and velocities.
- Counting-sort spatial grid for neighbor lookup.
- PBF-style density and lambda computation.
- Position correction with softer overlap handling and reduced tensile response.
- Velocity reconstruction that blends projected and smoothed velocity instead of converting every correction directly into bounce.
- Rest-state damping and lower-restitution boundary response for calmer settling.

## Metal Backend

The Metal backend keeps the same external contract as the CPU backend.

- Shared `MTLBuffer` objects store particle state, grid data, and render metrics.
- Compute passes handle integration, cell assignment, particle scatter, density/lambda solve, correction, and finalization.
- Grid prefix sums currently happen on the CPU over shared memory to keep the implementation explicit and maintainable.
- Render snapshots are read from shared buffers and handed to the existing OpenGL renderer.

## Threading Model

- The main thread owns GLFW, OpenGL, ImGui, camera updates, and final rendering.
- A worker thread owns simulation stepping and snapshot publication.
- UI settings and impulses are pushed into the worker thread through a small handoff layer.

## Snapshot Contract

The renderer consumes `WaterRenderSnapshot`, which contains:

- Particle positions
- Compact metrics used for shading/debug views
- Basin and interaction-plane context
- A version counter for update tracking

This keeps solver memory layout private and lets backends evolve independently.
