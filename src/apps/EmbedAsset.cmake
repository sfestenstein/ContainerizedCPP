# EmbedAsset.cmake — embeds a static web asset (HTML/CSS/JS) as a C++ raw
# string literal, generated at build time so the binary stays self-contained
# (no external files needed at runtime).
#
# Two ways this file is used:
#   1. As a build-time custom command (wired up by embed_web_asset() below):
#        cmake -DASSET_SRC=<input> -DASSET_INC=<output> -P EmbedAsset.cmake
#   2. include()'d as a module, providing embed_web_asset() to set up (1).

if(DEFINED ASSET_SRC)
   file(READ "${ASSET_SRC}" ASSET_CONTENT)
   file(WRITE "${ASSET_INC}" "R\"ASSETRAW(\n${ASSET_CONTENT})ASSETRAW\"\n")
   return()
endif()

# embed_web_asset(TARGET_NAME REL_SRC)
#
# Generates ${REL_SRC}.inc (relative to CMAKE_CURRENT_BINARY_DIR) from
# ${REL_SRC} (relative to CMAKE_CURRENT_SOURCE_DIR) and creates a custom
# target named TARGET_NAME that depends on it. Add TARGET_NAME as a
# dependency of whichever executable #includes the generated .inc file.
function(embed_web_asset TARGET_NAME REL_SRC)
   set(_src ${CMAKE_CURRENT_SOURCE_DIR}/${REL_SRC})
   set(_inc ${CMAKE_CURRENT_BINARY_DIR}/${REL_SRC}.inc)
   get_filename_component(_inc_dir ${_inc} DIRECTORY)
   file(MAKE_DIRECTORY ${_inc_dir})

   add_custom_command(
      OUTPUT  ${_inc}
      COMMAND ${CMAKE_COMMAND}
         -DASSET_SRC=${_src}
         -DASSET_INC=${_inc}
         -P ${CMAKE_CURRENT_FUNCTION_LIST_FILE}
      DEPENDS ${_src} ${CMAKE_CURRENT_FUNCTION_LIST_FILE}
      COMMENT "Embedding ${REL_SRC} into C++ include"
   )
   add_custom_target(${TARGET_NAME} DEPENDS ${_inc})
endfunction()
