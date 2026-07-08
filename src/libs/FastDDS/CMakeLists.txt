# =============================================================================
# FastDDS Library - CMakeLists.txt
#
# Uses eProsima Fast DDS for topic-based pub/sub, mirroring the shape of
# CycloneDDSLib. Unlike CycloneDDSLib, this module does not bundle its own
# IDL/message types — consumers bring their own IDL and compile it into a
# library via the fastddsgen_generate() CMake function (cmake/FastDDSGen.cmake).
# =============================================================================

cmake_minimum_required(VERSION 3.25)

project(FastDDSLib CXX)

find_package(fastrtps QUIET)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FASTDDS QUIET IMPORTED_TARGET fastrtps)

set(FASTDDS_LINK_TARGET)
if(TARGET fastrtps)
   set(FASTDDS_LINK_TARGET fastrtps)
elseif(TARGET fastrtps::fastrtps)
   set(FASTDDS_LINK_TARGET fastrtps::fastrtps)
elseif(TARGET FastDDS::fastrtps)
   set(FASTDDS_LINK_TARGET FastDDS::fastrtps)
elseif(TARGET PkgConfig::FASTDDS)
   set(FASTDDS_LINK_TARGET PkgConfig::FASTDDS)
endif()

if(NOT FASTDDS_LINK_TARGET)
   message(FATAL_ERROR
      "Could not resolve a FastDDS/FastRTPS CMake target. "
      "Expected one of: fastrtps, fastrtps::fastrtps, FastDDS::fastrtps, PkgConfig::FASTDDS. "
      "Install libfastrtps-dev (and optionally fastdds-tools/fastddsgen)."
   )
endif()

add_library(${PROJECT_NAME} STATIC FastDDSTopicConfig.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
   ${FASTDDS_LINK_TARGET}
   CommonUtils
)

