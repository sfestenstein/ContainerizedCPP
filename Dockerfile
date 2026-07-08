# syntax=docker/dockerfile:1.7
# =============================================================================
# ContainerizedCPP Development Container
#
# Apt-first dependency strategy:
# - Prefer Ubuntu 24.04 packages for faster, simpler, reproducible builds.
# - Build only missing dependencies from source (Zyre, CycloneDDS-CXX, Crow,
#   FastCDR/foonathan_memory/Fast-DDS/fastddsgen — pinned to the same versions
#   as Dockerfile.rocky9 rather than apt's older fastrtps/fastcdr packages).
#
# Build:
#   docker build -t ContainerizedCPP-dev .
#
# Run:
#   docker run -v $(pwd):/workspace -w /workspace -it ContainerizedCPP-dev
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
ARG FASTCDR_VERSION=v1.0.28
ARG FOONATHAN_MEMORY_VERSION=vendor-1.4.1
ARG FASTDDS_VERSION=v2.14.4
ARG FASTDDSGEN_VERSION=v3.3.0

# ---- Base environment -------------------------------------------------------
ENV DEBIAN_FRONTEND=noninteractive
ENV CONTAINERIZEDCPP_CONTAINER_BUILD=1
ENV CMAKE_PREFIX_PATH=/usr/local:/usr
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig
ENV IMAGE_DEFAULT_CC=gcc
ENV IMAGE_DEFAULT_CXX=g++

# Ubuntu's default ldconfig config already searches /usr/local/lib, but some
# source-built shared libs here (e.g. Fast-DDS) do not embed their own RPATH
# for transitive dependencies (e.g. foonathan_memory) — make the search path
# explicit so runtime linking doesn't depend on that default staying true.
RUN printf '/usr/local/lib\n/usr/local/lib64\n' > /etc/ld.so.conf.d/usrlocal.conf

# ---- Toolchain + apt dependencies ------------------------------------------
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
  --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
  apt-get update && apt-get install -y --no-install-recommends \
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
  openjdk-17-jdk-headless

RUN if [ ! -e /usr/bin/ninja-build ]; then ln -s /usr/bin/ninja /usr/bin/ninja-build; fi

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

# ---- FastCDR (not packaged in Ubuntu 24.04 at the version Fast-DDS needs) ---
RUN git clone --depth 1 --branch ${FASTCDR_VERSION} \
  https://github.com/eProsima/Fast-CDR.git /tmp/fastcdr \
  && cmake -S /tmp/fastcdr -B /tmp/fastcdr/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  && cmake --build /tmp/fastcdr/build \
  && cmake --install /tmp/fastcdr/build \
  && ldconfig \
  && rm -rf /tmp/fastcdr

# ---- foonathan_memory (required by Fast-DDS) --------------------------------
RUN git clone --depth 1 --branch ${FOONATHAN_MEMORY_VERSION} \
  https://github.com/eProsima/memory.git /tmp/foonathan_memory \
  && cmake -S /tmp/foonathan_memory -B /tmp/foonathan_memory/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DFOONATHAN_MEMORY_BUILD_EXAMPLES=OFF \
  -DFOONATHAN_MEMORY_BUILD_TESTS=OFF \
  -DFOONATHAN_MEMORY_BUILD_TOOLS=OFF \
  && cmake --build /tmp/foonathan_memory/build \
  && cmake --install /tmp/foonathan_memory/build \
  && ldconfig \
  && rm -rf /tmp/foonathan_memory

# ---- Fast-DDS (depends on FastCDR + Asio + tinyxml2, both via apt) ---------
RUN git clone --depth 1 --branch ${FASTDDS_VERSION} \
  https://github.com/eProsima/Fast-DDS.git /tmp/fastdds \
  && cmake -S /tmp/fastdds -B /tmp/fastdds/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DCOMPILE_TOOLS=OFF \
  -DBUILD_DOCUMENTATION=OFF \
  && cmake --build /tmp/fastdds/build \
  && cmake --install /tmp/fastdds/build \
  && ldconfig \
  && rm -rf /tmp/fastdds

# ---- fastddsgen (built via Gradle wrapper — requires Java) -------------------
RUN --mount=type=cache,target=/root/.gradle,sharing=locked \
  git clone --depth 1 --branch ${FASTDDSGEN_VERSION} \
  https://github.com/eProsima/Fast-DDS-Gen.git /tmp/fastddsgen \
  && cd /tmp/fastddsgen \
  && ./gradlew assemble --no-daemon -q \
  && mkdir -p /usr/local/share/fastddsgen/java \
  && cp build/libs/fastddsgen.jar /usr/local/share/fastddsgen/java/ \
  && printf '#!/bin/bash\nexec java -jar /usr/local/share/fastddsgen/java/fastddsgen.jar "$@"\n' \
  > /usr/local/bin/fastddsgen \
  && chmod +x /usr/local/bin/fastddsgen \
  && cd / && rm -rf /tmp/fastddsgen

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

RUN echo "alias ll='ls -alhF --color=auto'" >> /etc/bash.bashrc \
  && echo "alias lt='ls -alorth --color=auto'" >> /etc/bash.bashrc 

WORKDIR /workspace
