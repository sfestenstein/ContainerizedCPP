# =============================================================================
# GrpcGenerate.cmake
#
# Generates protobuf + gRPC C++ bindings from a .proto file and wraps them in
# a linkable static library target. No precedent existed for protobuf/gRPC
# code generation in this repo before GrpcChatLogger; mirrors the
# idlcxx_generate/RadarDDSMessages pattern used for CycloneDDS IDL-generated
# code (own .clang-tidy override, since generated code isn't held to the
# project's warning bar).
# =============================================================================

if(TARGET protobuf::protoc)
   set(_grpcgen_protoc_command protobuf::protoc)
else()
   find_program(Protobuf_PROTOC_EXECUTABLE protoc REQUIRED)
   set(_grpcgen_protoc_command ${Protobuf_PROTOC_EXECUTABLE})
endif()

if(TARGET gRPC::grpc_cpp_plugin)
   set(_grpcgen_plugin_path $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
else()
   find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)
   set(_grpcgen_plugin_path ${GRPC_CPP_PLUGIN_EXECUTABLE})
endif()

# add_grpc_library(<target> PROTO <path/to/file.proto>)
#
# Generates <file>.pb.{h,cc} and <file>.grpc.pb.{h,cc} into
# ${CMAKE_CURRENT_BINARY_DIR}/generated and compiles them into a STATIC
# library <target> that PUBLIC-links protobuf::libprotobuf and gRPC::grpc++,
# with the generated headers on its PUBLIC include path.
function(add_grpc_library TARGET_NAME)
   cmake_parse_arguments(ARG "" "PROTO" "" ${ARGN})
   if(NOT ARG_PROTO)
      message(FATAL_ERROR "add_grpc_library(${TARGET_NAME}): PROTO <file> is required")
   endif()

   get_filename_component(_proto_abs ${ARG_PROTO} ABSOLUTE)
   get_filename_component(_proto_dir ${_proto_abs} DIRECTORY)
   get_filename_component(_proto_name ${_proto_abs} NAME_WE)

   set(_gen_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
   file(MAKE_DIRECTORY ${_gen_dir})

   set(_pb_h       ${_gen_dir}/${_proto_name}.pb.h)
   set(_pb_cc      ${_gen_dir}/${_proto_name}.pb.cc)
   set(_grpc_pb_h  ${_gen_dir}/${_proto_name}.grpc.pb.h)
   set(_grpc_pb_cc ${_gen_dir}/${_proto_name}.grpc.pb.cc)

   add_custom_command(
      OUTPUT ${_pb_h} ${_pb_cc} ${_grpc_pb_h} ${_grpc_pb_cc}
      COMMAND ${_grpcgen_protoc_command}
         --cpp_out=${_gen_dir}
         --grpc_out=${_gen_dir}
         --plugin=protoc-gen-grpc=${_grpcgen_plugin_path}
         -I ${_proto_dir}
         ${_proto_abs}
      DEPENDS ${_proto_abs}
      COMMENT "Generating gRPC/protobuf sources for ${_proto_name}.proto"
      VERBATIM
   )

   add_library(${TARGET_NAME} STATIC ${_pb_cc} ${_grpc_pb_cc})
   target_include_directories(${TARGET_NAME} PUBLIC ${_gen_dir})
   target_link_libraries(${TARGET_NAME} PUBLIC protobuf::libprotobuf gRPC::grpc++)

   # Generated code isn't held to the project's warning/clang-tidy bar
   # (mirrors RadarDDSMessages' handling of CycloneDDS IDL-generated code).
   file(WRITE "${_gen_dir}/.clang-tidy" "---\nChecks: '-*'\nHeaderFilterRegex: ''\n")
   set_target_properties(${TARGET_NAME} PROPERTIES CXX_CLANG_TIDY "")
endfunction()
