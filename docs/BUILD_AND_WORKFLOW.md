# Build And Workflow

## Canonical Build Directories

- `build/`
  Default local development build.
- `build-perf/`
  Performance and benchmark-oriented build.
- `build-release/`
  Production-oriented release build.

These directories are local artifacts and are ignored by Git.

## Preset Workflow

Configure and build the default development target:

```bash
cmake --preset default
cmake --build --preset default
```

Configure and build the performance target:

```bash
cmake --preset perf
cmake --build --preset perf
```

Configure and build the release target:

```bash
cmake --preset release
cmake --build --preset release
```

## Runtime Commands

Run the app:

```bash
./build/FluidSimulation
```

Run the app with Metal selected:

```bash
./build/FluidSimulation --metal
```

Run the benchmark path:

```bash
./build-perf/FluidSimulation --benchmark
```

Run the Metal benchmark path:

```bash
./build-perf/FluidSimulation --benchmark --metal
```

## Workflow Guidance

- Use `build/` for day-to-day iteration.
- Use `build-perf/` when comparing timing output or profiling.
- Use `build-release/` for cleaner release-style builds.
