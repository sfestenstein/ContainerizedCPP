# =============================================================================
# FastDDSGen.cmake
#
# Compiles arbitrary OMG IDL files into Fast DDS C++ types via eProsima's
# `fastddsgen` tool, and wraps the generated sources in a static library
# target that links FastDDSLib.
#
# Usage:
#    fastddsgen_generate(
#       TARGET MyMessages
#       FILES ${CMAKE_CURRENT_SOURCE_DIR}/idl/MyType.idl ...
#       [INCLUDE_DIRS <extra -I paths for cross-file IDL includes>]
#    )
#    target_link_libraries(MyLib PUBLIC MyMessages)
# =============================================================================

function(fastddsgen_generate)
   set(options)
   set(oneValueArgs TARGET)
   set(multiValueArgs FILES INCLUDE_DIRS)
   cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

   if(NOT ARG_TARGET)
      message(FATAL_ERROR "fastddsgen_generate: TARGET is required")
   endif()
   if(NOT ARG_FILES)
      message(FATAL_ERROR "fastddsgen_generate: FILES is required")
   endif()

   find_program(FASTDDSGEN_EXECUTABLE fastddsgen)
   if(NOT FASTDDSGEN_EXECUTABLE)
      message(FATAL_ERROR
         "fastddsgen_generate: could not find the 'fastddsgen' executable. "
         "It is built from source by Dockerfile / Dockerfile.rocky9."
      )
   endif()

   set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/fastdds-gen/${ARG_TARGET}")
   file(MAKE_DIRECTORY "${_out_dir}")

   # Every IDL file's own directory is an implicit -I, so #include "Other.idl"
   # resolves regardless of which top-level file pulled it in.
   set(_include_dirs)
   foreach(_idl ${ARG_FILES})
      get_filename_component(_idl_dir "${_idl}" DIRECTORY)
      list(APPEND _include_dirs "${_idl_dir}")
   endforeach()
   list(APPEND _include_dirs ${ARG_INCLUDE_DIRS})
   list(REMOVE_DUPLICATES _include_dirs)

   set(_include_args)
   foreach(_dir ${_include_dirs})
      list(APPEND _include_args -I "${_dir}")
   endforeach()

   set(_generated_sources)
   set(_generated_outputs)
   foreach(_idl ${ARG_FILES})
      get_filename_component(_name "${_idl}" NAME_WE)
      list(APPEND _generated_sources
         "${_out_dir}/${_name}.cxx"
         "${_out_dir}/${_name}PubSubTypes.cxx"
      )
      list(APPEND _generated_outputs
         "${_out_dir}/${_name}.cxx"
         "${_out_dir}/${_name}.h"
         "${_out_dir}/${_name}PubSubTypes.cxx"
         "${_out_dir}/${_name}PubSubTypes.h"
         "${_out_dir}/${_name}CdrAux.hpp"
         "${_out_dir}/${_name}CdrAux.ipp"
      )
   endforeach()

   # -cdr v1: the container's Fast-DDS is built against Fast-CDR 1.0.28 (the
   # v1 API). fastddsgen defaults to v2-style generated code, which does not
   # compile against that version — this is not a stylistic choice.
   add_custom_command(
      OUTPUT ${_generated_outputs}
      COMMAND ${FASTDDSGEN_EXECUTABLE} -replace -cdr v1 -d "${_out_dir}" ${_include_args} ${ARG_FILES}
      DEPENDS ${ARG_FILES}
      WORKING_DIRECTORY "${_out_dir}"
      COMMENT "Generating Fast DDS types for target ${ARG_TARGET}"
   )

   add_library(${ARG_TARGET} STATIC ${_generated_sources})

   target_include_directories(${ARG_TARGET} PUBLIC
      $<BUILD_INTERFACE:${_out_dir}>
   )

   target_link_libraries(${ARG_TARGET} PUBLIC FastDDSLib)

   # fastddsgen's enum (de)serialization uses C-style casts (e.g.
   # `(uint32_t)m_status`) — this is generated, not project, code, so silence
   # just the warnings it actually triggers rather than the project's
   # stricter global warning set.
   target_compile_options(${ARG_TARGET} PRIVATE -Wno-old-style-cast)
endfunction()
