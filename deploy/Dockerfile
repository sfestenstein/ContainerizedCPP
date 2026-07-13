# =============================================================================
# ContainerizedCPP Deployment Image (multi-stage)
#
# Multi-stage build: uses the dev image (with all deps pre-compiled) as the
# build stage, then copies only the binaries and shared libraries into a
# minimal Ubuntu runtime image.
#
# Prerequisite — build the dev image once (cached, rarely changes):
#   docker build -t containerizedcpp-dev .
#
# Then build and run via Compose:
#   docker compose -f deploy/docker-compose.yml up --build
# =============================================================================

# ---- Stage 1: Build the project using the dev image ------------------------
FROM containerizedcpp-dev AS builder

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

COPY . /workspace
WORKDIR /workspace

# Patch CycloneDDS C++ 0.10.5 headers for GCC 13+ / C++20 compatibility
RUN bash deploy/fix-cyclonedds-headers.sh

# Build only the targets needed for deployment (not tests)
RUN cmake --preset release \
    && cmake --build --preset release \
       --target OmniscopeDds RadarDDSWorkstation RadarDDSRadar

# ---- Stage 2: Slim runtime image -------------------------------------------
FROM ubuntu:24.04 AS runtime

# Runtime dependencies for our shared libraries:
#   libssl3      — OpenSSL runtime for Cyclone DDS transitive deps
#   zlib1g       — compression runtime used by transitive deps
#   libstdc++6   — C++ standard library (already in base, listed for clarity)
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       libssl3 \
       zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Shared libraries: source-built third-party deps (CycloneDDS-CXX, etc.)
COPY --from=builder /usr/local/lib/lib*.so* /usr/local/lib/

# Shared libraries: apt-installed third-party deps not present in the slim
# runtime base (spdlog, fmt, Cyclone DDS C library, iceoryx shared-memory libs)
COPY --from=builder /usr/lib/x86_64-linux-gnu/libspdlog.so* /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/libddsc.so* /usr/lib/x86_64-linux-gnu/libiceoryx_*.so* /usr/local/lib/

# Shared libraries: project's own (libCommonUtils.so)
COPY --from=builder /workspace/build/release/lib/lib*.so* /usr/local/lib/

# Application binaries
COPY --from=builder /workspace/build/release/bin/OmniscopeDds       /usr/local/bin/
COPY --from=builder /workspace/build/release/bin/RadarDDSWorkstation /usr/local/bin/
COPY --from=builder /workspace/build/release/bin/RadarDDSRadar       /usr/local/bin/

RUN ldconfig

# Default: show what's available
CMD ["bash", "-c", "echo 'Available: OmniscopeDds, RadarDDSWorkstation, RadarDDSRadar'; exec bash"]
