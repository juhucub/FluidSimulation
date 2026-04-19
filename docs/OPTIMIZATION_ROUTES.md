# Optimization Routes

## Current Optimizations

- Structure-of-arrays particle state for hot-loop memory access.
- Counting-sort uniform grid for neighbor locality.
- Multithreaded CPU passes for integration and solver work.
- Compact render snapshot payloads instead of full solver state exposure.
- Metal compute backend for GPU execution on macOS.
- Separate benchmark mode for repeatable timing runs.

## Recommended Next Routes

### CPU Path

- Reuse temporary allocations inside grid rebuild and prefix-sum stages.
- Reduce repeated full-neighbor traversals where metrics can be shared safely.
- Add benchmark scene presets for calm-rest, repeated impulse, and wall-slash cases.
- Profile thread chunk sizing against particle count and core count.

### Metal Path

- Move the grid prefix-sum stage fully onto GPU if profiling shows the CPU shared-memory prefix sum as a bottleneck.
- Add indirect command or batched dispatch tuning for large particle counts.
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
- smoothing length
- rest density
- damping and viscosity
- boundary response
- backend scheduling
