# =============================================================================
# DdsSnooper — dynamic DDS topic radar and message snooper
# =============================================================================

# ---------------------------------------------------------------------------
# Embed the web UI HTML as a C++ raw-string include.
# Uses the same monitor.html as Omniscope for Phase 1; Phase 3 will
# introduce a DdsSnooper-specific UI with QoS badges and hex/decoded views.
# ---------------------------------------------------------------------------
set(MONITOR_HTML_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../Omniscope/web/monitor.html)
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
   COMMENT "Embedding monitor.html into DdsSnooper C++ include"
)
add_custom_target(DdsSnooperHtml DEPENDS ${MONITOR_HTML_INC})

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(DdsSnooper
   main.cpp
   TransportDdsSnooper.cpp
)
add_dependencies(DdsSnooper DdsSnooperHtml)

# OmniscopeCore provides OmniscopeApp, PlaybackEngine, ITransport, and Crow.
# Crow headers trigger clang-tidy false positives — disable for this target.
set_target_properties(DdsSnooper PROPERTIES CXX_CLANG_TIDY "")

target_include_directories(DdsSnooper PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${CMAKE_CURRENT_BINARY_DIR}   # web/monitor.html.inc
)

target_link_libraries(DdsSnooper
   PRIVATE OmniscopeCore CycloneDDSLib CommonUtils
)
