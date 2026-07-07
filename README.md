# StarterCpp

A production-ready C++20 project template using CMake presets, containerized development,
comprehensive tests, and CI automation.

## Features

- C++20 with GCC/Clang support
- CMake 3.25+ with preset-based workflows
- Container-first dependency management
- Google Test unit tests
- Protocol Buffers + gRPC support
- ZeroMQ/CZMQ/Zyre messaging
- Eclipse Cyclone DDS integration
- Optional FastDDS integration target for new apps
- Omniscope web-based traffic inspector
- VITA 49.2 codec utilities
- clang-format, clang-tidy, sanitizers, and coverage

## Quick Start

### Recommended: Dev Container

```bash
# 1) Open repository in VS Code
# 2) Run: Dev Containers: Rebuild and Reopen in Container

cmake --preset container-debug
cmake --build --preset container-debug
ctest --preset container-debug
```

### Additional Container Presets

```bash
cmake --preset container-release
cmake --build --preset container-release

cmake --preset container-debug-fastdds
cmake --build --preset container-debug-fastdds
```

### Coverage

```bash
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
cmake --build --preset coverage --target coverage
```

Coverage is primarily intended for CI or advanced validation flows. The day-to-day
developer path is container-debug/container-release.

## Running Applications

```bash
./build/container-debug/bin/ZyreSubscriber
./build/container-debug/bin/ZyrePublisher
./build/container-debug/bin/DDSSubscriber
./build/container-debug/bin/DDSPublisher
./build/container-debug/bin/Omniscope
```

## Presets

| Preset                      | Purpose                                  |
| --------------------------- | ---------------------------------------- |
| `debug`                     | Development build with sanitizers        |
| `debug-with-clang-tidy`     | Development build with clang-tidy        |
| `release`                   | Optimized build                          |
| `coverage`                  | Coverage-instrumented build              |
| `container-debug`           | Container development build              |
| `container-release`         | Container release build                  |
| `container-debug-fastdds`   | Container debug build with FastDDS lib   |
| `container-release-fastdds` | Container release build with FastDDS lib |
| `ci-linux`                  | CI build with coverage                   |

## CMake Options

| Option              | Default                | Description                                    |
| ------------------- | ---------------------- | ---------------------------------------------- |
| `BUILD_TESTS`       | ON                     | Build unit tests                               |
| `ENABLE_COVERAGE`   | OFF (coverage presets) | Enable coverage report targets                 |
| `BUILD_FASTDDS_LIB` | OFF                    | Build optional `FastDDSLib` integration target |

## Documentation

- [Build Guide](docs/BUILD.md)
- [Development Guide](docs/DEVELOPMENT.md)
- [Design Notes](docs/DESIGN.md)

## License

[MIT License](LICENSE)
