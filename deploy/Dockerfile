# =============================================================================
# StarterCpp Deployment Image (multi-stage)
#
# Multi-stage build: uses the dev image (with all deps pre-compiled) as the
# build stage, then copies only the binaries and shared libraries into a
# minimal Ubuntu runtime image.
#
# Prerequisite — build the dev image once (cached, rarely changes):
#   docker build -t startercpp-dev .
#
# Then build and run via Compose:
#   docker compose -f deploy/docker-compose.yml up --build
# =============================================================================

# ---- Stage 1: Build the project using the dev image ------------------------
FROM startercpp-dev AS builder

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

COPY . /workspace
WORKDIR /workspace

# Patch CycloneDDS C++ 0.10.5 headers for GCC 13+ / C++20 compatibility
RUN bash deploy/fix-cyclonedds-headers.sh

# Build only the targets needed for deployment (not tests/benchmarks)
RUN cmake --preset container-release \
    && cmake --build --preset container-release \
       --target DDSPublisher DDSSubscriber Omniscope ZyrePublisher ZyreSubscriber \
              GrpcServer GrpcClient

# ---- Stage 2: Slim runtime image -------------------------------------------
FROM ubuntu:24.04 AS runtime

# Runtime dependencies for our shared libraries:
#   libsystemd0  — required by CZMQ/Zyre (sd-bus integration)
#   libssl3      — OpenSSL runtime for gRPC/Cyclone DDS transitive deps
#   zlib1g       — compression runtime used by protobuf/gRPC transitive deps
#   libstdc++6   — C++ standard library (already in base, listed for clarity)
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       libsystemd0 \
       libssl3 \
         libfastcdr1t64 \
         libfastrtps2.11t64 \
       zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Shared libraries: third-party deps (cyclonedds, spdlog, zmq, etc.)
COPY --from=builder /usr/local/lib/lib*.so* /usr/local/lib/

# Shared libraries: project's own (libCommonUtils.so, libPubSubLib.so)
COPY --from=builder /workspace/build/container-release/lib/lib*.so* /usr/local/lib/

# Application binaries (only the three we built)
COPY --from=builder /workspace/build/container-release/bin/DDSPublisher   /usr/local/bin/
COPY --from=builder /workspace/build/container-release/bin/DDSSubscriber  /usr/local/bin/
COPY --from=builder /workspace/build/container-release/bin/Omniscope      /usr/local/bin/
COPY --from=builder /workspace/build/container-release/bin/ZyrePublisher  /usr/local/bin/
COPY --from=builder /workspace/build/container-release/bin/ZyreSubscriber /usr/local/bin/
COPY --from=builder /workspace/build/container-release/bin/GrpcServer     /usr/local/bin/
COPY --from=builder /workspace/build/container-release/bin/GrpcClient     /usr/local/bin/

RUN ldconfig

# Default: show what's available
CMD ["bash", "-c", "echo 'Available: DDSPublisher, DDSSubscriber, Omniscope, ZyrePublisher, ZyreSubscriber, GrpcServer, GrpcClient'; exec bash"]
