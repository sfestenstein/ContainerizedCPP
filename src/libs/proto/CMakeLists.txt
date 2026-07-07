# =============================================================================
# Protocol Buffer Library - CMakeLists.txt
# =============================================================================

project(ProtoLib CXX)

# Prefer the protobuf target when it is available, otherwise locate protoc on PATH.
if(TARGET protobuf::protoc)
   get_target_property(PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)
endif()

if(NOT PROTOC_EXECUTABLE)
   find_program(PROTOC_EXECUTABLE protoc REQUIRED)
endif()

if(NOT PROTOC_EXECUTABLE)
   message(FATAL_ERROR "Could not find protoc executable. Install protobuf-compiler or provide protobuf::protoc.")
endif()

message(STATUS "Using protoc: ${PROTOC_EXECUTABLE}")

# Proto source files
file (GLOB PROTO_FILES ${CMAKE_CURRENT_LIST_DIR}/proto-messages/*.proto)

# Output directories for generated files
set(PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GENERATED_DIR})

# Generate C++ files from proto files
set(PROTO_SRCS)
set(PROTO_HDRS)

foreach(PROTO_FILE ${PROTO_FILES})
   get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

   set(PROTO_SRC ${PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.cc)
   set(PROTO_HDR ${PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.h)

   list(APPEND PROTO_SRCS ${PROTO_SRC})
   list(APPEND PROTO_HDRS ${PROTO_HDR})

   add_custom_command(
      OUTPUT ${PROTO_SRC} ${PROTO_HDR}
      COMMAND ${PROTOC_EXECUTABLE}
         --proto_path=${CMAKE_CURRENT_LIST_DIR}/proto-messages
         --cpp_out=${PROTO_GENERATED_DIR}
         ${PROTO_FILE}
      DEPENDS ${PROTO_FILE} ${PROTO_FILES}
      COMMENT "Generating C++ files from ${PROTO_NAME}.proto"
      VERBATIM
   )
endforeach()

# Create the proto library
add_library(${PROJECT_NAME} STATIC
   ${PROTO_SRCS}
   ${PROTO_HDRS}
)

add_library(StarterCpp::proto ALIAS ${PROJECT_NAME})

# Include directories (SYSTEM suppresses warnings in generated headers)
target_include_directories(${PROJECT_NAME}
   SYSTEM PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
      $<INSTALL_INTERFACE:include>
)

# Hand-written headers (not SYSTEM — show warnings for our own code)
target_include_directories(${PROJECT_NAME}
   PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
      $<INSTALL_INTERFACE:include>
)

# Link protobuf
target_link_libraries(${PROJECT_NAME}
   PUBLIC
      protobuf::libprotobuf
)

# Suppress warnings in generated code
target_compile_options(${PROJECT_NAME} PRIVATE -w)

# Set properties
set_target_properties(${PROJECT_NAME} PROPERTIES
   OUTPUT_NAME "proto"
   POSITION_INDEPENDENT_CODE ON
   CXX_CLANG_TIDY ""
)
