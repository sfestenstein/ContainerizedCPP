# =============================================================================
# ContainerizedCPP - Root CMakeLists.txt
# =============================================================================
cmake_minimum_required(VERSION 3.25)

# Project definition
project(ContainerizedCPP
   VERSION 1.0.0
   DESCRIPTION "A robust C++ Containerized starter project template"
   LANGUAGES CXX
)

# =============================================================================
# Global Settings
# =============================================================================

# Add cmake/ to module path for Coverage.cmake etc.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# =============================================================================
# Options
# =============================================================================

option(BUILD_TESTS "Build unit tests" ON)

# =============================================================================
# Compiler Warnings
# =============================================================================

# Define warning flags
set(GCC_WARNINGS
   -Wall
   -Wextra
   -Wshadow
   -Wnon-virtual-dtor
   -Wold-style-cast
   -Wcast-align
   -Wunused
   -Woverloaded-virtual
   -Wconversion
   -Wsign-conversion
   -Wnull-dereference
   -Wdouble-promotion
   -Wformat=2
   -Wimplicit-fallthrough
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
   add_compile_options(${GCC_WARNINGS})

   # GCC 13 can emit false positives in fmt/spdlog internals when optimized.
   # Keep strict warnings for project code while suppressing these noisy externals.
   add_compile_options(
      $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-Wno-array-bounds>
      $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-Wno-stringop-overflow>
   )
else()
   message(FATAL_ERROR "ContainerizedCPP supports only GNU or Clang compilers.")
endif()

# =============================================================================
# Dependencies
# =============================================================================

# Find package-config-based dependencies provided by the container or system
find_package(spdlog REQUIRED)
find_package(CycloneDDS REQUIRED)

# CycloneDDS-CXX imports may reference this target when built with iceoryx.
find_package(iceoryx_binding_c QUIET CONFIG)
if(NOT TARGET iceoryx_binding_c::iceoryx_binding_c)
   find_package(PkgConfig QUIET)
   if(PkgConfig_FOUND)
      pkg_check_modules(ICEORYX_BINDING_C QUIET IMPORTED_TARGET iceoryx_binding_c)
      if(TARGET PkgConfig::ICEORYX_BINDING_C)
         add_library(iceoryx_binding_c::iceoryx_binding_c INTERFACE IMPORTED)
         target_link_libraries(iceoryx_binding_c::iceoryx_binding_c
            INTERFACE PkgConfig::ICEORYX_BINDING_C
         )
      endif()
   endif()
endif()

find_package(CycloneDDS-CXX REQUIRED)
find_package(Crow REQUIRED)

# opentelemetry-cpp's exported CMake config references the Protobuf::libprotobuf
# and gRPC::grpc++ imported targets directly, so they must be resolved first.
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(opentelemetry-cpp CONFIG REQUIRED)

if(BUILD_TESTS)
   find_package(GTest REQUIRED)

   # Some distro GTest packages provide gtest but not a dedicated gmock target.
   if(NOT TARGET GTest::gmock AND TARGET GTest::gtest)
      add_library(GTest::gmock INTERFACE IMPORTED)
      target_link_libraries(GTest::gmock INTERFACE GTest::gtest)
   endif()
endif()

# =============================================================================
# Output Directories
# =============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# =============================================================================
# Subdirectories
# =============================================================================

# Libraries
add_subdirectory(src/libs/CommonUtils)
add_subdirectory(src/libs/DdsCore)
add_subdirectory(src/libs/Observability)
add_subdirectory(src/libs/CycloneDDS)

# Applications
add_subdirectory(src/apps)

# Tests
if(BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests/CommonUtilsTests)
   add_subdirectory(tests/DDSTests)
   add_subdirectory(tests/OmniscopeDdsTests)

   # Unified coverage target — runs all tests, generates a combined report
   include(Coverage)
   add_unified_coverage_target(
      NAME    coverage
      TITLE   "ContainerizedCPP Coverage Report"
   )
endif()

# =============================================================================
# Installation (Optional)
# =============================================================================

include(GNUInstallDirs)

install(TARGETS CommonUtils
   EXPORT ContainerizedCppTargets
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== ContainerizedCPP Configuration ===")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build tests:      ${BUILD_TESTS}")
message(STATUS "================================")
message(STATUS "")
