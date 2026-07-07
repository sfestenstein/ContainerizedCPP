# Build Guide

This document describes the container-first build flow for StarterCpp.

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
cmake --preset container-debug
cmake --build --preset container-debug
ctest --preset container-debug
```

### Docker CLI

```bash
docker build -t startercpp-dev .
docker run --rm -it -v "$PWD":/workspace -w /workspace startercpp-dev \
  bash -lc "cmake --preset container-debug && cmake --build --preset container-debug && ctest --preset container-debug"
```

## Preset Builds

### Debug (container toolchain)

```bash
cmake --preset container-debug
cmake --build --preset container-debug
ctest --preset container-debug
```

### Debug with clang-tidy

```bash
cmake --preset debug-with-clang-tidy
cmake --build --preset debug-with-clang-tidy
ctest --preset debug-with-clang-tidy
```

### Release

```bash
cmake --preset container-release
cmake --build --preset container-release
```

### Coverage

```bash
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
cmake --build --preset coverage --target coverage
```

Coverage presets are primarily for CI and verification workflows.

### FastDDS Integration Target (Container)

```bash
cmake --preset container-debug-fastdds
cmake --build --preset container-debug-fastdds
```

This builds the optional `FastDDSLib` target used for upcoming FastDDS-backed apps.

## Build Options

| Option              | Default                | Description                               |
| ------------------- | ---------------------- | ----------------------------------------- |
| `BUILD_TESTS`       | ON                     | Build unit tests                          |
| `ENABLE_COVERAGE`   | OFF (coverage presets) | Enable coverage report targets            |
| `BUILD_FASTDDS_LIB` | OFF                    | Build optional FastDDS integration target |

## Running Applications

After building with `debug` preset:

```bash
./build/container-debug/bin/ZyreSubscriber
./build/container-debug/bin/ZyrePublisher
./build/container-debug/bin/DDSSubscriber
./build/container-debug/bin/DDSPublisher
./build/container-debug/bin/Omniscope
```

## Troubleshooting

### Protobuf Compiler Not Found

```bash
which protoc
which grpc_cpp_plugin
```

If missing, install system protobuf/grpc development packages or use the dev container.

In this repository, using the dev container is the supported path.

### Compiler Unsupported

StarterCpp supports only GNU and Clang compilers.

### FastDDS Target Configure Fails

If `container-debug-fastdds` fails with missing `fastrtps`, rebuild the dev image so new
FastDDS packages are present:

```bash
docker build -t startercpp-dev .
```
