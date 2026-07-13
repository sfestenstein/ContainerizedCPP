# =============================================================================
# OmniscopeDds — web-based DDS traffic inspector with dynamic topic discovery
# =============================================================================

# ---------------------------------------------------------------------------
# Embed the monitor HTML as a C++ raw-string include
# ---------------------------------------------------------------------------
set(MONITOR_HTML_SRC ${CMAKE_CURRENT_SOURCE_DIR}/web/monitor.html)
set(MONITOR_HTML_INC ${CMAKE_CURRENT_BINARY_DIR}/web/monitor.html.inc)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/web)

add_custom_command(
   OUTPUT  ${MONITOR_HTML_INC}
   COMMAND ${CMAKE_COMMAND}
      -DHTML_SRC=${MONITOR_HTML_SRC}
      -DHTML_INC=${MONITOR_HTML_INC}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../EmbedHtml.cmake
   DEPENDS ${MONITOR_HTML_SRC}
           ${CMAKE_CURRENT_SOURCE_DIR}/../EmbedHtml.cmake
   COMMENT "Embedding monitor.html into C++ include"
)
add_custom_target(OmniscopeDdsHtml DEPENDS ${MONITOR_HTML_INC})

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(OmniscopeDds
   main.cpp
   OmniscopeApp.cpp
   PlaybackEngine.cpp
   TransportDds.cpp
)
add_dependencies(OmniscopeDds OmniscopeDdsHtml)

# Crow headers trigger clang-tidy false positives — disable for this target.
set_target_properties(OmniscopeDds PROPERTIES CXX_CLANG_TIDY "")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   target_compile_options(
      OmniscopeDds
      PRIVATE
         $<$<CONFIG:Release>:-Wno-null-dereference>
   )
endif()

target_include_directories(OmniscopeDds PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}   # ITransport.h, OmniscopeApp.h, PlaybackEngine.h, TransportDds.h
   ${CMAKE_CURRENT_BINARY_DIR}   # web/monitor.html.inc
)

target_link_libraries(OmniscopeDds
   PRIVATE Crow::Crow CommonUtils CycloneDDSLib
)
