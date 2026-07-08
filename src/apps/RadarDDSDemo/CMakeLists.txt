# =============================================================================
# RadarDDSDemo - CMakeLists.txt
#
# Two-app demo (Workstation + Radar) exercising FastDDSLib with a distinct
# QoS profile per topic (see RadarTopics.h). Own IDL, not the CycloneDDS
# notional message set.
# =============================================================================

project(RadarDDSDemo CXX)

include(FastDDSGen)

fastddsgen_generate(
   TARGET RadarDDSMessages
   FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/idl/RadarMessageHeader.idl
      ${CMAKE_CURRENT_SOURCE_DIR}/idl/Command.idl
      ${CMAKE_CURRENT_SOURCE_DIR}/idl/CommandStatus.idl
      ${CMAKE_CURRENT_SOURCE_DIR}/idl/RadarTrack.idl
      ${CMAKE_CURRENT_SOURCE_DIR}/idl/ComponentStatus.idl
      ${CMAKE_CURRENT_SOURCE_DIR}/idl/RadarAlert.idl
)

add_library(RadarTopics STATIC RadarTopics.cpp)

target_include_directories(RadarTopics PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RadarTopics PUBLIC
   RadarDDSMessages
   FastDDSLib
)

add_executable(RadarDDSWorkstation Workstation.cpp)
target_link_libraries(RadarDDSWorkstation PRIVATE RadarTopics CommonUtils)

add_executable(RadarDDSRadar Radar.cpp)
target_link_libraries(RadarDDSRadar PRIVATE RadarTopics CommonUtils)
