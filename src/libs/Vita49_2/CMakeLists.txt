cmake_minimum_required(VERSION 3.20)
project(Vita49_2 CXX)

file(GLOB Vita49_2_source "*.cpp")
file(GLOB Vita49_2_headers "*.h")

add_library(Vita49_2 SHARED ${Vita49_2_source} ${Vita49_2_headers})

target_include_directories(Vita49_2
   PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
)
