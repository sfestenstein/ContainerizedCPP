# =============================================================================
# FastDDS Library - CMakeLists.txt
#
# Initial integration target for eProsima FastDDS/Fast RTPS.
# This target is optional and currently intended for new apps (e.g. future
# DdsSnooper backend variants) while existing DDS apps continue to use
# CycloneDDSLib.
# =============================================================================

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

add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE
   ${FASTDDS_LINK_TARGET}
   CommonUtils
)

target_compile_definitions(${PROJECT_NAME} INTERFACE
   STARTERCPP_HAS_FASTDDS=1
)
