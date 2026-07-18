# =============================================================================
# DdsCore - CMakeLists.txt
#
# Vendor-agnostic DDS abstractions: interfaces and plain data types for
# dynamic topic discovery and raw-bytes pub/sub. Header-only, and
# deliberately depends on nothing but the standard library — no CycloneDDS,
# no RTI, no vendor SDK of any kind. Concrete implementations live in
# vendor-specific modules (e.g. src/libs/CycloneDDS) that link against this.
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(DdsCore CXX)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
   ${CMAKE_CURRENT_SOURCE_DIR}     # bare includes within DdsCore source files
   ${CMAKE_CURRENT_SOURCE_DIR}/..  # "DdsCore/..." style includes for consumers
)
