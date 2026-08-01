project(ObservabilityTests)

include(GoogleTest)

enable_testing()

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${UNIT_TEST_SOURCE})

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      Observability
      GTest::gtest
      GTest::gtest_main
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
   NAME    ObservabilityCoverage
   TITLE   "Observability Coverage Report"
   DEPENDS ${PROJECT_NAME}
)
