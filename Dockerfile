# syntax=docker/dockerfile:1.7
# =============================================================================
# ContainerizedCPP Development Container
#
# Apt-first dependency strategy:
# - Prefer Ubuntu 24.04 packages for faster, simpler, reproducible builds.
# - Build only missing dependencies from source (CycloneDDS-CXX, Crow).
#
# Build:
#   docker build -t containerizedcpp-dev .
#
# Run:
#   docker run -v $(pwd):/workspace -w /workspace -it containerizedcpp-dev
#
# Build inside container:
#   cmake --preset debug-san
#   cmake --build --preset debug-san
#   ctest --preset debug-san
# =============================================================================

FROM ubuntu:24.04

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# ---- Source-build fallbacks for dependencies not available via apt ----------
ARG CYCLONEDDS_CXX_VERSION=0.10.5
ARG CROW_VERSION=v1.2.0

# ---- Base environment -------------------------------------------------------
ENV DEBIAN_FRONTEND=noninteractive
ENV CONTAINERIZEDCPP_CONTAINER_BUILD=1
ENV CMAKE_PREFIX_PATH=/usr/local:/usr
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig
ENV IMAGE_DEFAULT_CC=gcc
ENV IMAGE_DEFAULT_CXX=g++

# Ubuntu's default ldconfig config already searches /usr/local/lib, but some
# source-built shared libs here do not embed their own RPATH for transitive
# dependencies — make the search path explicit so runtime linking doesn't
# depend on that default staying true.
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
  libiceoryx-binding-c-dev \
  cyclonedds-dev \
  cyclonedds-tools \
  libgtest-dev \
  libgmock-dev

RUN if [ ! -e /usr/bin/ninja-build ]; then ln -s /usr/bin/ninja /usr/bin/ninja-build; fi

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
