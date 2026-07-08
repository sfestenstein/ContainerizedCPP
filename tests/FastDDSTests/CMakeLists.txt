project(FastDDSTests)

include(FastDDSGen)
include(GoogleTest)

enable_testing()

fastddsgen_generate(
   TARGET FastDDSTestMessages
   FILES ${CMAKE_CURRENT_SOURCE_DIR}/idl/FastDDSTestMessage.idl
)

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${UNIT_TEST_SOURCE})

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      FastDDSLib
      FastDDSTestMessages
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
   NAME    FastDDSCoverage
   TITLE   "FastDDS Coverage Report"
   DEPENDS ${PROJECT_NAME}
)
