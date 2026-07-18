project(OmniscopeDdsTests)

include(GoogleTest)

enable_testing()

file(GLOB UNIT_TEST_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(${PROJECT_NAME}
   ${UNIT_TEST_SOURCE}
   ${CMAKE_SOURCE_DIR}/src/apps/OmniscopeDds/RawSampleJsonCodec.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
   ${CMAKE_SOURCE_DIR}/src/apps/OmniscopeDds
)

target_link_libraries(${PROJECT_NAME}
   PRIVATE
      DdsCore
      Crow::Crow
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
   NAME    OmniscopeDdsCoverage
   TITLE   "OmniscopeDds Coverage Report"
   DEPENDS ${PROJECT_NAME}
)
