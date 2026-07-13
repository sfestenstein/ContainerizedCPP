# ContainerizedCPP

A C++20 DDS showcase built with CMake presets, containerized development,
comprehensive tests, and CI automation. Fuses a web-based DDS traffic
inspector with dynamic topic discovery, recording, and playback
(OmniscopeDds) with a two-app QoS demonstration (RadarDDSDemo) — all on a
single DDS implementation (Eclipse Cyclone DDS).

## Features

- C++20 with GCC/Clang support
- CMake 3.25+ with preset-based workflows
- Container-first dependency management
- Google Test unit tests
- Eclipse Cyclone DDS integration (sole DDS implementation), with an
  event-driven (WaitSet-based) generic DDSSubscriber<T>
- OmniscopeDds — web-based dynamic DDS topic discovery, live monitoring,
  recording, and wire-level playback
- RadarDDSDemo — two-app QoS profile demonstration (Best Effort vs Reliable,
  Volatile vs TransientLocal, KeepLast vs KeepAll)
- clang-format, clang-tidy, sanitizers, and coverage

## Quick Start

### Recommended: Dev Container

```bash
# 1) Open repository in VS Code
# 2) Run: Dev Containers: Rebuild and Reopen in Container

cmake --preset debug-san
cmake --build --preset debug-san
ctest --preset debug-san
```

### Release Build

```bash
cmake --preset release
cmake --build --preset release
```

### Coverage

```bash
cmake --preset debug-coverage
cmake --build --preset debug-coverage
ctest --preset debug-coverage
cmake --build --preset debug-coverage --target coverage
```

Coverage is primarily intended for CI or advanced validation flows. The day-to-day
developer path is debug-san/release.

## Running Applications

```bash
./build/debug-san/bin/OmniscopeDds 0 8080
./build/debug-san/bin/RadarDDSRadar 0
./build/debug-san/bin/RadarDDSWorkstation 0
```

## Presets

| Preset            | Purpose                             |
| ----------------- | ------------------------------------ |
| `debug-san`       | Debug build with ASan/UBSan          |
| `release`         | Release build                        |
| `debug-coverage`  | Debug build with coverage instrumentation |
| `ci-linux`        | CI build with coverage                |

## CMake Options

| Option            | Default                | Description                    |
| ----------------- | ---------------------- | ------------------------------ |
| `BUILD_TESTS`     | ON                     | Build unit tests               |
| `ENABLE_COVERAGE` | OFF (coverage presets) | Enable coverage report targets |

## Documentation

- [Build Guide](docs/BUILD.md)
- [Development Guide](docs/DEVELOPMENT.md)
- [Design Notes](docs/DESIGN.md)

## License

[MIT License](LICENSE)
