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

- A private ping-pong `ParticleSoA` layout for hot simulation state.
- Cell-reordered particle buffers produced during grid rebuild so neighbor-heavy passes walk contiguous cell ranges directly.
- Stable particle ids carried through reorders so snapshot publication and interaction behavior stay deterministic.
- Counting-sort uniform grid metadata for cell starts, counts, and active-cell tracking.
- A staged DFSPH-compatible split solve:
  external-force integration, density-plus-alpha evaluation, density pressure iterations on predicted positions, a post-correction grid rebuild, provisional velocity reconstruction, divergence cleanup iterations, and final velocity/metric synthesis.
- Threaded hot passes over cell-local storage instead of index-indirected neighbor walks.
- Snapshot rebuild in stable-id order so the renderer contract stays unchanged even though the solver state is reordered internally.

The current CPU density stage still rebuilds the reordered grid each density iteration. That keeps the corrected-prediction solve stable and visually closer to the previous baseline, but it is also the main remaining CPU cost to tune after this architectural landing.

## Metal Backend

The Metal backend keeps the same external contract as the CPU backend.

- Shared `MTLBuffer` objects store particle state, grid data, solver scalars, and render metrics.
- Compute passes now mirror the CPU solve structure:
  integration, cell assignment, GPU exclusive scan, particle scatter, density-plus-alpha evaluation, density pressure update, density correction, provisional velocity update, divergence evaluation, divergence pressure update, divergence correction, and finalization.
- Grid prefix sums run fully on the GPU through a pragmatic multi-pass exclusive scan instead of a CPU prefix sum over shared buffers.
- Render snapshots are still read from shared buffers and handed to the existing OpenGL renderer without changing the snapshot contract.

The GPU scan is intentionally simple and reviewable rather than fully optimized. It removes the CPU round-trip from the grid stage and keeps the backend architecture ready for later dispatch and memory-tuning work.

## Benchmark Harness

Benchmark mode lives in `src/app/main.cpp` and now supports deterministic scene presets across both backends.

- `steady-state`
  Original box-spawn benchmark path with short warmup and measurement windows.
- `calm-rest`
  No impulses, longer warmup, and a longer steady measurement window for settling behavior.
- `repeated-impulse`
  Alternating periodic disturbances through the same runtime impulse path used by interactive input.
- `wall-slash`
  A deterministic moving impulse near one wall to stress asymmetry, shear, and boundary behavior.

The CSV output now includes scene identity plus divergence timing and divergence-error metrics so density-only regressions are easier to distinguish from velocity-compression regressions.

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
