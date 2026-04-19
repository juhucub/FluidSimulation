# App Functionality

## Overview

The application is an interactive fluid sandbox focused on real-time experimentation with particle-based water behavior.

## User-Facing Features

- Orbit, pan, and zoom camera controls.
- Mouse-driven fluid disturbances on the water surface plane.
- Live tuning for particle layout, physical parameters, damping, viscosity, boundary behavior, and impulse strength.
- Runtime backend selection between CPU and Metal where available.
- Debug views for density, velocity, pressure, and interaction energy.
- Frame-time and solver-phase instrumentation in the control panel.

## Simulation Modes

- `CPU`:
  General-purpose baseline backend using multithreaded C++ passes.
- `Metal`:
  macOS-specific compute backend for simulation-heavy workloads.

## Benchmark Modes

- `--benchmark`:
  Runs benchmark cases and prints CSV timing output.
- `--benchmark --metal`:
  Runs the same benchmark path against the Metal backend on macOS.

## Data Flow

1. The simulation backend advances particle state.
2. A compact render snapshot is produced.
3. The main thread hands snapshot data to the OpenGL renderer.
4. ImGui presents current stats and tuning controls.

## Main User Goal

Provide a fast environment for iterating on fluid realism, responsiveness, solver tuning, and backend performance without changing the renderer/UI architecture.
