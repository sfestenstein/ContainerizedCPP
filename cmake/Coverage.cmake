# =============================================================================
# Coverage.cmake — Reusable code coverage helpers
# =============================================================================
#
# Provides add_coverage_target() to create per-library coverage targets and
# add_unified_coverage_target() to merge them into a single report.
#
# Usage in a test CMakeLists.txt:
#
#   include(Coverage)
#   add_coverage_target(
#      NAME  CommonUtilsCoverage
#      TITLE "CommonUtils Coverage Report"
#      DEPENDS CommonUtilsTests
#   )
#
# In the root CMakeLists.txt:
#
#   include(Coverage)
#   add_unified_coverage_target(
#      NAME     coverage
#      TITLE    "StarterCpp Coverage Report"
#      TARGETS  CommonUtilsCoverage PubSubCoverage DDSCoverage Vita49_2Coverage
#   )
# =============================================================================

# Guard against multiple inclusion
if(_COVERAGE_CMAKE_INCLUDED)
   return()
endif()
set(_COVERAGE_CMAKE_INCLUDED TRUE)

find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)
find_program(GCOV_EXECUTABLE gcov)

set(LCOV_IGNORE_ERRORS --ignore-errors inconsistent,mismatch,unused,negative)

# --------------------------------------------------------------------------
# add_coverage_target(NAME <target> TITLE <title> DEPENDS <test-executable>)
#
# Creates a custom target that:
#   1. Zeros gcov counters
#   2. Runs ctest
#   3. Captures coverage with lcov
#   4. Filters out system/test/build paths
#   5. Generates an HTML report via genhtml
# --------------------------------------------------------------------------
function(add_coverage_target)
   cmake_parse_arguments(COV "" "NAME;TITLE" "DEPENDS" ${ARGN})

   if(NOT ENABLE_COVERAGE)
      return()
   endif()

   if(NOT LCOV_EXECUTABLE OR NOT GENHTML_EXECUTABLE)
      message(WARNING "lcov/genhtml not found. ${COV_NAME} target disabled.")
      add_custom_target(${COV_NAME}
         COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
         DEPENDS ${COV_DEPENDS}
         COMMENT "Running tests (${COV_NAME} HTML disabled — lcov/genhtml not found)"
      )
      return()
   endif()

   set(_info_file ${CMAKE_BINARY_DIR}/${COV_NAME}.info)

   add_custom_target(${COV_NAME}
      COMMAND ${LCOV_EXECUTABLE}
         --zerocounters --directory ${CMAKE_BINARY_DIR}
         ${LCOV_IGNORE_ERRORS}
      COMMAND ${CMAKE_COMMAND} -E echo "Running tests for ${COV_NAME}..."
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
      COMMAND ${LCOV_EXECUTABLE}
         --capture
         --directory ${CMAKE_BINARY_DIR}
         --output-file ${_info_file}
         --gcov-tool ${GCOV_EXECUTABLE}
         ${LCOV_IGNORE_ERRORS}
      COMMAND ${LCOV_EXECUTABLE}
         --remove ${_info_file}
         /usr/* ${CMAKE_BINARY_DIR}/* */test/* */tests/*
         --output-file ${_info_file}
         ${LCOV_IGNORE_ERRORS}
      COMMAND ${GENHTML_EXECUTABLE}
         ${_info_file}
         --output-directory ${CMAKE_BINARY_DIR}/${COV_NAME}
         --title "${COV_TITLE}"
         --legend --demangle-cpp
         ${LCOV_IGNORE_ERRORS}
      COMMAND ${CMAKE_COMMAND} -E echo
         "Coverage report: ${CMAKE_BINARY_DIR}/${COV_NAME}/index.html"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS ${COV_DEPENDS}
      COMMENT "Generating ${COV_NAME} coverage report"
   )
endfunction()

# --------------------------------------------------------------------------
# add_unified_coverage_target(NAME <target> TITLE <title>
#                             TARGETS <cov1> <cov2> ...)
#
# Creates a single target that:
#   1. Zeros gcov counters
#   2. Runs ALL tests
#   3. Captures + filters coverage
#   4. Generates a combined HTML report
#   5. Prints a summary to the console
#
# The per-library targets are listed as dependencies so their test
# executables are built first, but only ONE ctest invocation runs.
# --------------------------------------------------------------------------
function(add_unified_coverage_target)
   cmake_parse_arguments(COV "" "NAME;TITLE" "TARGETS" ${ARGN})

   if(NOT ENABLE_COVERAGE)
      return()
   endif()

   if(NOT LCOV_EXECUTABLE OR NOT GENHTML_EXECUTABLE)
      message(WARNING "lcov/genhtml not found. Unified coverage target disabled.")
      return()
   endif()

   set(_info_file ${CMAKE_BINARY_DIR}/${COV_NAME}.info)

   add_custom_target(${COV_NAME}
      COMMAND ${LCOV_EXECUTABLE}
         --zerocounters --directory ${CMAKE_BINARY_DIR}
         ${LCOV_IGNORE_ERRORS}
      COMMAND ${CMAKE_COMMAND} -E echo "Running all tests for unified coverage..."
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
      COMMAND ${LCOV_EXECUTABLE}
         --capture
         --directory ${CMAKE_BINARY_DIR}
         --output-file ${_info_file}
         --gcov-tool ${GCOV_EXECUTABLE}
         ${LCOV_IGNORE_ERRORS}
      COMMAND ${LCOV_EXECUTABLE}
         --remove ${_info_file}
         /usr/* ${CMAKE_BINARY_DIR}/* */test/* */tests/*
         --output-file ${_info_file}
         ${LCOV_IGNORE_ERRORS}
      COMMAND ${GENHTML_EXECUTABLE}
         ${_info_file}
         --output-directory ${CMAKE_BINARY_DIR}/coverage
         --title "${COV_TITLE}"
         --legend --demangle-cpp
         ${LCOV_IGNORE_ERRORS}
      COMMAND ${LCOV_EXECUTABLE} --list ${_info_file} ${LCOV_IGNORE_ERRORS}
      COMMAND ${CMAKE_COMMAND} -E echo
         "Unified coverage report: ${CMAKE_BINARY_DIR}/coverage/index.html"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating unified coverage report"
   )
endfunction()
