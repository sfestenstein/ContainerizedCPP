# Build Guide

This document describes the container-first build flow for ContainerizedCPP.

## Prerequisites

### Recommended (all hosts)

- Docker Desktop or Docker Engine
- VS Code with Dev Containers extension (optional, but recommended)

## Container Build (Primary)

### VS Code Dev Container

1. Open the repository in VS Code.
2. Run Dev Containers: Rebuild and Reopen in Container.
3. Build and test:

```bash
cmake --preset debug-san
cmake --build --preset debug-san
ctest --preset debug-san
```

### Docker CLI

```bash
docker build -t containerizedcpp-dev .
docker run --rm -it -v "$PWD":/workspace -w /workspace containerizedcpp-dev \
  bash -lc "cmake --preset debug-san && cmake --build --preset debug-san && ctest --preset debug-san"
```

## Preset Builds

### Debug (with sanitizers)

```bash
cmake --preset debug-san
cmake --build --preset debug-san
ctest --preset debug-san
```

### Release

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

Coverage presets are primarily for CI and verification workflows.

## Build Options

| Option            | Default                | Description                    |
| ----------------- | ---------------------- | ------------------------------ |
| `BUILD_TESTS`     | ON                     | Build unit tests               |
| `ENABLE_COVERAGE` | OFF (coverage presets) | Enable coverage report targets |

## Running Applications

After building with the `debug-san` preset:

```bash
./build/debug-san/bin/OmniscopeDds 0 8080
./build/debug-san/bin/RadarDDSRadar 0
./build/debug-san/bin/RadarDDSWorkstation 0
```

## Troubleshooting

### Compiler Unsupported

ContainerizedCPP supports only GNU and Clang compilers.

### CycloneDDS-CXX Target Configure Fails

If `debug-san` fails with missing `CycloneDDS-CXX` or `ddscxx`, rebuild the dev
image so it's freshly built from source:

```bash
docker build -t containerizedcpp-dev .
```
