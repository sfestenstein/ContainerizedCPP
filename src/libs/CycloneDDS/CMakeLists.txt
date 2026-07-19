# =============================================================================
# DDS Library - CMakeLists.txt
#
# Uses Eclipse Cyclone DDS (C++ binding) for topic-based pub/sub. This
# library is type-agnostic — DDSPublisher<T>/DDSSubscriber<T> are templates,
# and BlobSertype/BuiltinTopicReader work without any compiled-in IDL types.
# Each app supplies its own IDL and generates its own message types (see
# src/apps/RadarDDSDemo/CMakeLists.txt for the idlcxx_generate() pattern).
# =============================================================================

cmake_minimum_required(VERSION 3.25)

project(CycloneDDSLib CXX)

find_package(CycloneDDS REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)

# ---------------------------------------------------------------------------
# Fix IMPORTED_LOCATION for CycloneDDS::idlc in non-Release configs.
# Some package configurations expose only one imported location, so map it
# across the active build types we use in this project.
# ---------------------------------------------------------------------------
if(TARGET CycloneDDS::idlc)
   get_target_property(_idlc_loc CycloneDDS::idlc IMPORTED_LOCATION)
   if(_idlc_loc)
      set_target_properties(CycloneDDS::idlc PROPERTIES
         IMPORTED_LOCATION_DEBUG "${_idlc_loc}"
         IMPORTED_LOCATION_RELEASE "${_idlc_loc}"
      )
   endif()
endif()

# ---------------------------------------------------------------------------
# CycloneDDSLib target (compiled library — .cpp sources + header-only templates)
# ---------------------------------------------------------------------------

file(GLOB CYCLONE_DDS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} STATIC ${CYCLONE_DDS_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
   CycloneDDS-CXX::ddscxx
   CommonUtils
   DdsCore
   Observability
)
