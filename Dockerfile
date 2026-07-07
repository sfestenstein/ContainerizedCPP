# =============================================================================
# StarterCpp Development Container
#
# Apt-first dependency strategy:
# - Prefer Ubuntu 24.04 packages for faster, simpler, reproducible builds.
# - Build only missing dependencies from source (Zyre, CycloneDDS-CXX, Crow).
#
# Build:
#   docker build -t startercpp-dev .
#
# Run:
#   docker run -v $(pwd):/workspace -w /workspace -it startercpp-dev
#
# Build inside container:
#   cmake --preset container-debug
#   cmake --build --preset container-debug
#   ctest --preset container-debug
# =============================================================================

FROM ubuntu:24.04

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# ---- Source-build fallbacks for dependencies not available via apt ----------
ARG ZYRE_VERSION=v2.0.1
ARG CYCLONEDDS_CXX_VERSION=0.10.5
ARG CROW_VERSION=v1.2.0

# ---- Base environment -------------------------------------------------------
ENV DEBIAN_FRONTEND=noninteractive
ENV STARTERCPP_CONTAINER_BUILD=1
ENV CMAKE_PREFIX_PATH=/usr/local:/usr
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig

# ---- Toolchain + apt dependencies ------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      ninja-build \
      python3 \
      git \
      ca-certificates \
      pkg-config \
      clangd \
      clang-tidy \
      lcov \
      gcovr \
      libsystemd-dev \
      libasio-dev \
      libssl-dev \
      zlib1g-dev \
      libtinyxml2-dev \
      libspdlog-dev \
      libprotobuf-dev \
      protobuf-compiler \
      libgrpc++-dev \
      protobuf-compiler-grpc \
      libfastrtps-dev \
      libfastcdr-dev \
      fastdds-tools \
      fastddsgen \
      libre2-dev \
      libc-ares-dev \
      libzmq3-dev \
      cppzmq-dev \
      libczmq-dev \
      libiceoryx-binding-c-dev \
      cyclonedds-dev \
      cyclonedds-tools \
      libgtest-dev \
      libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

# ---- Zyre (not packaged in Ubuntu 24.04) -----------------------------------
RUN git clone --depth 1 --branch ${ZYRE_VERSION} \
      https://github.com/zeromq/zyre.git /tmp/zyre \
    && cmake -S /tmp/zyre -B /tmp/zyre/build -G Ninja \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX=/usr/local \
       -DZYRE_BUILD_STATIC=ON \
       -DZYRE_BUILD_TESTS=OFF \
       -DENABLE_DRAFTS=ON \
    && cmake --build /tmp/zyre/build \
    && cmake --install /tmp/zyre/build \
    && ldconfig \
    && rm -rf /tmp/zyre

# ---- CycloneDDS-CXX (CycloneDDS C package comes from apt) ------------------
RUN git clone --depth 1 --branch ${CYCLONEDDS_CXX_VERSION} \
      https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git /tmp/cyclonedds-cxx \
    && cmake -S /tmp/cyclonedds-cxx -B /tmp/cyclonedds-cxx/build -G Ninja \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX=/usr/local \
       -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
       -DBUILD_TESTING=OFF \
       -DBUILD_EXAMPLES=OFF \
       -DCMAKE_PREFIX_PATH=/usr:/usr/local \
    && cmake --build /tmp/cyclonedds-cxx/build \
    && cmake --install /tmp/cyclonedds-cxx/build \
    && ldconfig \
    && rm -rf /tmp/cyclonedds-cxx

# ---- Fix CycloneDDS-CXX 0.10.x headers for GCC 13+ / C++20 -----------------
RUN sed -i 's/~Reference<DELEGATE>()/~Reference()/' \
      /usr/local/include/ddscxx/dds/core/detail/ReferenceImpl.hpp \
    && sed -i 's/~Topic<T>()/~Topic()/' \
      /usr/local/include/ddscxx/dds/topic/detail/TTopicImpl.hpp \
    && sed -i 's/~DataReader<T>()/~DataReader()/' \
      /usr/local/include/ddscxx/dds/sub/detail/TDataReaderImpl.hpp \
    && sed -i 's/~DataWriter<T>()/~DataWriter()/' \
      /usr/local/include/ddscxx/dds/pub/detail/DataWriterImpl.hpp

# ---- Crow (not packaged in Ubuntu 24.04) -----------------------------------
RUN git clone --depth 1 --branch ${CROW_VERSION} \
      https://github.com/CrowCpp/Crow.git /tmp/crow \
    && cmake -S /tmp/crow -B /tmp/crow/build -G Ninja \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX=/usr/local \
       -DCROW_BUILD_EXAMPLES=OFF \
       -DCROW_BUILD_TESTS=OFF \
    && cmake --build /tmp/crow/build \
    && cmake --install /tmp/crow/build \
    && rm -rf /tmp/crow

RUN ldconfig

WORKDIR /workspace
