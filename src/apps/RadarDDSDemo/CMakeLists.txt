# =============================================================================
# RadarDDSDemo - CMakeLists.txt
#
# Two-app demo (Workstation + Radar) exercising CycloneDDSLib with a distinct
# QoS profile per topic (see RadarTopics.h). Own IDL, not the CycloneDDS
# library's notional message set (src/libs/CycloneDDS/idl).
# =============================================================================

project(RadarDDSDemo CXX)

find_package(CycloneDDS-CXX REQUIRED)

file(GLOB RADAR_IDL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/idl/*.idl")

idlcxx_generate(TARGET RadarDDSMessages FILES ${RADAR_IDL_FILES})

# IDL-generated code triggers clang-tidy warnings. Drop a .clang-tidy
# config next to the generated files that disables all checks so that
# any target compiling them stays warning-free (mirrors src/libs/CycloneDDS).
get_target_property(_radar_dds_gen_dir RadarDDSMessages INTERFACE_INCLUDE_DIRECTORIES)
if(_radar_dds_gen_dir)
   file(WRITE "${_radar_dds_gen_dir}/.clang-tidy"
      "---\nChecks: '-*,misc-definitions-in-headers'\nHeaderFilterRegex: ''\n"
   )
endif()
set_target_properties(RadarDDSMessages PROPERTIES CXX_CLANG_TIDY "")

add_library(RadarTopics STATIC RadarTopics.cpp)

target_include_directories(RadarTopics PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RadarTopics PUBLIC
   RadarDDSMessages
   CycloneDDSLib
   CommonUtils
)

add_executable(RadarDDSWorkstation Workstation.cpp)
target_link_libraries(RadarDDSWorkstation PRIVATE RadarTopics CommonUtils)

add_executable(RadarDDSRadar Radar.cpp)
target_link_libraries(RadarDDSRadar PRIVATE RadarTopics CommonUtils)
