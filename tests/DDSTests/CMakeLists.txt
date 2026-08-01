project(DDSTests)

include(GoogleTest)

enable_testing()

find_package(CycloneDDS-CXX REQUIRED)

file(GLOB DDS_TEST_IDL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/idl/*.idl")
idlcxx_generate(TARGET DDSTestMessages FILES ${DDS_TEST_IDL_FILES})

# IDL-generated code triggers clang-tidy warnings (mirrors src/libs/CycloneDDS).
get_target_property(_dds_test_gen_dir DDSTestMessages INTERFACE_INCLUDE_DIRECTORIES)
if(_dds_test_gen_dir)
   file(WRITE "${_dds_test_gen_dir}/.clang-tidy"
      "---\nChecks: '-*,misc-definitions-in-headers'\nHeaderFilterRegex: ''\n"
   )
endif()
set_target_properties(DDSTestMessages PROPERTIES CXX_CLANG_TIDY "")

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${UNIT_TEST_SOURCE})

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      CycloneDDSLib
      DDSTestMessages
      CommonUtils
      # Already transitive via CycloneDDSLib -- listed explicitly now that
      # tests use Observability::FakeInterfaceMetrics/ScopedMetrics directly.
      Observability
      GTest::gtest
      GTest::gmock
)

# Discover tests for CTest
gtest_discover_tests(${PROJECT_NAME}
   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
   PROPERTIES
      LABELS "unit"
)

# =============================================================================
# Coverage Target
# =============================================================================

include(Coverage)
add_coverage_target(
   NAME    DDSCoverage
   TITLE   "DDS Coverage Report"
   DEPENDS ${PROJECT_NAME}
)
