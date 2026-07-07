project(DDSTests)

include(GoogleTest)

enable_testing()

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${UNIT_TEST_SOURCE})

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      CycloneDDSLib
      CommonUtils
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
