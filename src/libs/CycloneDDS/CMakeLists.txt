# =============================================================================
# DDS Library - CMakeLists.txt
#
# Uses Eclipse Cyclone DDS (C++ binding) for topic-based pub/sub.
# IDL files in the idl/ directory are automatically compiled into
# C++ types by the Cyclone DDS idlc compiler via IDLCXX_GENERATE().
# =============================================================================

cmake_minimum_required(VERSION 3.25)

project(CycloneDDSLib CXX)

find_package(CycloneDDS REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
# IDL code generation via Cyclone DDS build modules
# ---------------------------------------------------------------------------

file(GLOB IDL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/idl/*.idl")

idlcxx_generate(TARGET DDSMessages FILES ${IDL_FILES})

# IDL-generated code triggers clang-tidy warnings.  Drop a .clang-tidy
# config next to the generated files that disables all checks so that
# any target compiling them stays warning-free.
get_target_property(_dds_gen_dir DDSMessages INTERFACE_INCLUDE_DIRECTORIES)
if(_dds_gen_dir)
   file(WRITE "${_dds_gen_dir}/.clang-tidy"
      "---\nChecks: '-*,misc-definitions-in-headers'\nHeaderFilterRegex: ''\n"
   )
endif()
set_target_properties(DDSMessages PROPERTIES CXX_CLANG_TIDY "")

# ---------------------------------------------------------------------------
# JSON serialization helpers — auto-generated from .idl files
# ---------------------------------------------------------------------------

set(DDS_JSON_HELPERS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/DdsJsonHelpers.h")

add_custom_command(
   OUTPUT  ${DDS_JSON_HELPERS_OUTPUT}
   COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/generate_dds_json_helpers.py
      --idl-dir ${CMAKE_CURRENT_SOURCE_DIR}/idl
      --output  ${DDS_JSON_HELPERS_OUTPUT}
      --accessor-style method
   DEPENDS ${IDL_FILES}
           ${CMAKE_CURRENT_SOURCE_DIR}/generate_dds_json_helpers.py
   COMMENT "Generating DDS JSON helpers from IDL files"
)
add_custom_target(DdsJsonHelpersGen DEPENDS ${DDS_JSON_HELPERS_OUTPUT})

# ---------------------------------------------------------------------------
# CycloneDDSLib target (compiled library — .cpp sources + header-only templates)
# ---------------------------------------------------------------------------

file(GLOB CYCLONE_DDS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} STATIC ${CYCLONE_DDS_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
   $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

add_dependencies(${PROJECT_NAME} DdsJsonHelpersGen)

target_link_libraries(${PROJECT_NAME} PUBLIC
   DDSMessages
   CycloneDDS-CXX::ddscxx
   CommonUtils
)
