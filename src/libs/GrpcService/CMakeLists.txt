# =============================================================================
# GrpcService Library - CMakeLists.txt
#
# Generates gRPC stubs + protobuf messages from .proto files and packages
# them into a static library shared by the server and client test apps.
# =============================================================================

project(GrpcServiceLib CXX)

# ---- Locate protoc ----------------------------------------------------------
if(TARGET protobuf::protoc)
   get_target_property(GRPC_PROTOC protobuf::protoc IMPORTED_LOCATION)
endif()

if(NOT GRPC_PROTOC)
   find_program(GRPC_PROTOC protoc)
endif()

if(NOT GRPC_PROTOC)
   message(FATAL_ERROR "Could not find protoc for GrpcServiceLib")
endif()

# ---- Locate grpc_cpp_plugin -------------------------------------------------
if(TARGET gRPC::grpc_cpp_plugin)
   get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
endif()

if(NOT GRPC_CPP_PLUGIN)
   find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
endif()

if(NOT GRPC_CPP_PLUGIN)
   message(FATAL_ERROR "Could not find grpc_cpp_plugin for GrpcServiceLib")
endif()

message(STATUS "GrpcServiceLib — protoc: ${GRPC_PROTOC}")
message(STATUS "GrpcServiceLib — grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")

# ---- Generate from .proto ---------------------------------------------------

file(GLOB GRPC_PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)

set(GRPC_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GRPC_GENERATED_DIR})

set(GRPC_GEN_SRCS)
set(GRPC_GEN_HDRS)

foreach(PROTO_FILE ${GRPC_PROTO_FILES})
   get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

   set(PB_SRC   ${GRPC_GENERATED_DIR}/${PROTO_NAME}.pb.cc)
   set(PB_HDR   ${GRPC_GENERATED_DIR}/${PROTO_NAME}.pb.h)
   set(GRPC_SRC ${GRPC_GENERATED_DIR}/${PROTO_NAME}.grpc.pb.cc)
   set(GRPC_HDR ${GRPC_GENERATED_DIR}/${PROTO_NAME}.grpc.pb.h)

   list(APPEND GRPC_GEN_SRCS ${PB_SRC} ${GRPC_SRC})
   list(APPEND GRPC_GEN_HDRS ${PB_HDR} ${GRPC_HDR})

   add_custom_command(
      OUTPUT  ${PB_SRC} ${PB_HDR} ${GRPC_SRC} ${GRPC_HDR}
      COMMAND ${GRPC_PROTOC}
         --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
         --cpp_out=${GRPC_GENERATED_DIR}
         --grpc_out=${GRPC_GENERATED_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         ${PROTO_FILE}
      DEPENDS ${PROTO_FILE}
      COMMENT "Generating gRPC/protobuf C++ from ${PROTO_NAME}.proto"
      VERBATIM
   )
endforeach()

# ---- Static library ----------------------------------------------------------

add_library(${PROJECT_NAME} STATIC
   ${GRPC_GEN_SRCS}
   ${GRPC_GEN_HDRS}
)

# Generated headers — SYSTEM to suppress warnings in generated code
target_include_directories(${PROJECT_NAME}
   SYSTEM PUBLIC
      $<BUILD_INTERFACE:${GRPC_GENERATED_DIR}>
      $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}
   PUBLIC
      gRPC::grpc++
      protobuf::libprotobuf
)

# Suppress warnings in generated code
target_compile_options(${PROJECT_NAME} PRIVATE -w)

set_target_properties(${PROJECT_NAME} PROPERTIES
   OUTPUT_NAME "grpc_service"
   POSITION_INDEPENDENT_CODE ON
   CXX_CLANG_TIDY ""
)
