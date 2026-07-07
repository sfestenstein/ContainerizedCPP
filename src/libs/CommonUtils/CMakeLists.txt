cmake_minimum_required(VERSION 3.20)
project(CommonUtils CXX)

file(GLOB CommonUtil_source     "*.cpp")
file(GLOB CommonUtil_headers     "*.h")


add_library(CommonUtils SHARED ${CommonUtil_source} ${CommonUtil_headers})
target_link_libraries(CommonUtils PUBLIC spdlog::spdlog)

target_include_directories(CommonUtils
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}     # bare includes within CommonUtils source files
      ${CMAKE_CURRENT_SOURCE_DIR}/..  # "CommonUtils/..." style includes for consumers
)