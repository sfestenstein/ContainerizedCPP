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
ARG OTEL_CPP_VERSION=v1.14.2
# GRPC_VERSION: the earliest gRPC release with CMake support for the
# grpcpp_otel_plugin target (gRPC_BUILD_GRPCPP_OTEL_PLUGIN, added in v1.63.0)
# -- deliberately not the newest gRPC release, to stay as close as possible
# in vintage to the Abseil/Protobuf versions built alongside it below.
ARG GRPC_VERSION=v1.63.0

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
  libgmock-dev \
  libprotobuf-dev \
  protobuf-compiler \
  libgrpc++-dev \
  protobuf-compiler-grpc

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

# ---- gRPC with its native OpenTelemetry plugin (apt's libgrpc++-dev does --
# ---- not build this target) -------------------------------------------------
# Ubuntu 24.04's apt gRPC/Protobuf/Abseil packages stay installed untouched
# (see apt-get block above) -- both as a safety net and because they're what
# provides the transitive libc-ares-dev/libre2-dev packages used below. This
# block installs a *newer*, source-built gRPC/Protobuf/Abseil on top, into
# /usr/local, which CMAKE_PREFIX_PATH (set to "/usr/local:/usr" above)
# already prefers over the apt copies -- no CMakeLists.txt changes needed
# beyond linking the new gRPC::grpcpp_otel_plugin target.
#
# Getting the plugin requires resolving two circular dependencies:
#  1. opentelemetry-cpp's OTLP/gRPC exporter needs gRPC built first; gRPC's
#     otel plugin needs opentelemetry-cpp installed first to build -- so
#     gRPC is built twice (plugin off, then on) with the otel-cpp build
#     sandwiched in between.
#  2. gRPC's `-DgRPC_PROTOBUF_PROVIDER=package` hard-requires a CMake
#     *config*-mode Protobuf package, which Ubuntu 24.04's apt libprotobuf-dev
#     doesn't ship (only the older module-mode FindProtobuf.cmake) -- so
#     Protobuf is source-built too, from the exact version gRPC's own
#     submodule pins, which in turn requires a newer Abseil (20240116) than
#     apt's (20220623.1) -- so Abseil is source-built from that same pinned
#     commit as well. All three (Abseil, Protobuf, gRPC) end up consistently
#     linked against each other and against opentelemetry-cpp, avoiding the
#     two-copies-of-a-library ABI/ODR class of bug documented below for
#     WITH_ABSEIL.
RUN git clone --depth 1 --branch ${GRPC_VERSION} \
  https://github.com/grpc/grpc.git /tmp/grpc \
  && cd /tmp/grpc && git submodule update --init --depth 1 third_party/abseil-cpp

RUN cmake -S /tmp/grpc/third_party/abseil-cpp -B /tmp/grpc/absl-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DABSL_PROPAGATE_CXX_STD=ON \
  -DABSL_BUILD_TESTING=OFF \
  -DABSL_ENABLE_INSTALL=ON \
  && cmake --build /tmp/grpc/absl-build \
  && cmake --install /tmp/grpc/absl-build \
  && ldconfig

RUN cd /tmp/grpc && git submodule update --init --depth 1 third_party/protobuf \
  && cmake -S /tmp/grpc/third_party/protobuf -B /tmp/grpc/protobuf-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -Dprotobuf_BUILD_TESTS=OFF \
  -Dprotobuf_INSTALL=ON \
  -Dprotobuf_ABSL_PROVIDER=package \
  -DCMAKE_PREFIX_PATH=/usr:/usr/local \
  && cmake --build /tmp/grpc/protobuf-build \
  && cmake --install /tmp/grpc/protobuf-build \
  && ldconfig

# Pass 1: plugin off (opentelemetry-cpp isn't built yet).
RUN cmake -S /tmp/grpc -B /tmp/grpc/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DgRPC_BUILD_TESTS=OFF \
  -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=OFF \
  -DgRPC_PROTOBUF_PROVIDER=package \
  -DgRPC_ABSL_PROVIDER=package \
  -DgRPC_CARES_PROVIDER=package \
  -DgRPC_RE2_PROVIDER=package \
  -DgRPC_SSL_PROVIDER=package \
  -DgRPC_ZLIB_PROVIDER=package \
  -DCMAKE_PREFIX_PATH=/usr:/usr/local \
  && cmake --build /tmp/grpc/build \
  && cmake --install /tmp/grpc/build \
  && ldconfig

# ---- OpenTelemetry C++ SDK (not packaged in Ubuntu 24.04) ------------------
# Submodules are required: opentelemetry-proto (OTLP protobuf definitions)
# lives as a git submodule, not a vendored copy.
#
# -DWITH_ABSEIL=ON: without this, opentelemetry-cpp falls back to its own
# vendored "nostd" polyfill of a few absl:: types (variant, utility) for
# builds that don't want a real Abseil dependency. That polyfill declares
# its own `namespace absl { ... }`, which becomes ambiguous with the real
# Abseil the moment a single translation unit includes both an
# opentelemetry-cpp header and a gRPC header (gRPC pulls in real Abseil
# transitively) -- exactly what apps combining Observability:: with direct
# gRPC usage (e.g. GrpcChatLogger) need to do. Building against the same
# real Abseil installed above avoids the collision entirely.
#
# -DWITH_STL=ON: without this, opentelemetry::nostd::shared_ptr is
# opentelemetry-cpp's own hand-rolled ref-counted pointer, not an alias for
# std::shared_ptr. gRPC's OpenTelemetryPluginBuilder::SetMeterProvider()
# takes a real std::shared_ptr, so without this flag
# opentelemetry::metrics::Provider::GetMeterProvider()'s return type won't
# even convert to what the plugin API expects.
RUN git clone --recurse-submodules --shallow-submodules --depth 1 \
  --branch ${OTEL_CPP_VERSION} \
  https://github.com/open-telemetry/opentelemetry-cpp.git /tmp/opentelemetry-cpp \
  && cmake -S /tmp/opentelemetry-cpp -B /tmp/opentelemetry-cpp/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_TESTING=OFF \
  -DWITH_EXAMPLES=OFF \
  -DOPENTELEMETRY_INSTALL=ON \
  -DWITH_OTLP_GRPC=ON \
  -DWITH_OTLP_HTTP=OFF \
  -DWITH_ABSEIL=ON \
  -DWITH_STL=ON \
  -DCMAKE_PREFIX_PATH=/usr:/usr/local \
  && cmake --build /tmp/opentelemetry-cpp/build \
  && cmake --install /tmp/opentelemetry-cpp/build \
  && ldconfig \
  && rm -rf /tmp/opentelemetry-cpp

# Pass 2: plugin on, now that opentelemetry-cpp is installed and discoverable.
RUN cmake -S /tmp/grpc -B /tmp/grpc/build -G Ninja \
  -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=ON \
  -DCMAKE_PREFIX_PATH=/usr:/usr/local \
  && cmake --build /tmp/grpc/build \
  && cmake --install /tmp/grpc/build \
  && ldconfig \
  && rm -rf /tmp/grpc

RUN ldconfig

RUN echo "alias ll='ls -alhF --color=auto'" >> /etc/bash.bashrc \
  && echo "alias lt='ls -alorth --color=auto'" >> /etc/bash.bashrc 

WORKDIR /workspace
