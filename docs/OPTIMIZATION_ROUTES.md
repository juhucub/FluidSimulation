# Optimization Routes

## Current Optimizations

- Cell-reordered structure-of-arrays particle state for CPU hot-loop locality.
- Counting-sort uniform grid with cell starts, counts, and direct contiguous neighbor walks.
- Multithreaded CPU passes for integration and solver work.
- Compact render snapshot payloads instead of full solver state exposure.
- Metal compute backend for GPU execution on macOS.
- GPU-side exclusive scan for the Metal grid prefix-sum stage.
- Split density and divergence solver stages for a DFSPH-compatible path on both backends.
- Separate benchmark mode with deterministic scene presets for repeatable timing runs.

## Recommended Next Routes

### CPU Path

- Reduce reorder payload and reorder frequency inside density iterations without giving back the stability gained from corrected-prediction rebuilds.
- Reuse density and divergence warm-start state more aggressively when the local density error is already low.
- Profile thread chunk sizing against particle count and core count now that hot loops walk cell-local buffers directly.
- Consider staged early-out criteria for density and divergence iterations once regression coverage over the new scenes is stronger.

### Metal Path

- Tune scan block sizing and command-buffer batching now that the grid prefix sum is on GPU.
- Add indirect command or batched dispatch tuning for large particle counts.
- Reduce shared-buffer CPU touches further so the GPU path spends less time waiting on host-side stats reads.
- Expand GPU-side instrumentation if deeper backend timing visibility becomes necessary.

### Rendering Path

- Add optional render interpolation between snapshots to smooth visible motion at lower snapshot rates.
- Explore depth-aware splatting or screen-space fluid post-processing if more cohesive volume appearance is needed.
- Reduce visual over-emphasis of high-frequency motion before increasing solver complexity further.

## Stability-Focused Optimization

- Prefer routes that reduce corrective noise before routes that increase solver stiffness.
- Favor lossy boundary contact, viscosity coherence, and rest damping over collision-like separation behavior.
- Keep realism changes benchmarked against settling behavior, not just throughput.

## Benchmark Strategy

Use `--benchmark` and `--benchmark --metal` as the baseline verification loop whenever changing:

- particle counts
- benchmark scenes
- smoothing length
- rest density
- damping and viscosity
- boundary response
- backend scheduling
