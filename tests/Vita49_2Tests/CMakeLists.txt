project(Vita49_2Tests)

include(GoogleTest)

# Enable CTest
enable_testing()

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

# Add the source files
add_executable(${PROJECT_NAME} ${UNIT_TEST_SOURCE})

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      Vita49_2
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
   NAME    Vita49_2Coverage
   TITLE   "Vita49_2 Coverage Report"
   DEPENDS ${PROJECT_NAME}
)
