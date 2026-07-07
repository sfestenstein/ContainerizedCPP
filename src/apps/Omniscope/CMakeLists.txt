# =============================================================================
# Omniscope — web-based IPC traffic inspector
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
add_custom_target(OmniscopeHtml DEPENDS ${MONITOR_HTML_INC})

# ---------------------------------------------------------------------------
# OmniscopeCore — shared OBJECT library (OmniscopeApp + PlaybackEngine).
# Reused by both the Omniscope and DdsSnooper executables, avoiding source
# duplication. Transport implementations are NOT included here; each app
# links its own transports.
# ---------------------------------------------------------------------------
add_library(OmniscopeCore OBJECT
   OmniscopeApp.cpp
   PlaybackEngine.cpp
)
add_dependencies(OmniscopeCore OmniscopeHtml)

# Crow headers trigger clang-tidy false positives — disable for this target.
set_target_properties(OmniscopeCore PROPERTIES CXX_CLANG_TIDY "")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   target_compile_options(
      OmniscopeCore
      PRIVATE
         $<$<CONFIG:Release>:-Wno-null-dereference>
   )
endif()

target_include_directories(OmniscopeCore PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}   # ITransport.h, OmniscopeApp.h, PlaybackEngine.h
   ${CMAKE_CURRENT_BINARY_DIR}   # web/monitor.html.inc
)

target_link_libraries(OmniscopeCore PUBLIC
   Crow::Crow
   CommonUtils
)

# ---------------------------------------------------------------------------
# Omniscope executable — OmniscopeCore + DDS and Zyre transports
# ---------------------------------------------------------------------------
add_executable(Omniscope
   main.cpp
   TransportDds.cpp
   TransportZyre.cpp
)

set_target_properties(Omniscope PROPERTIES CXX_CLANG_TIDY "")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   target_compile_options(
      Omniscope
      PRIVATE
         $<$<CONFIG:Release>:-Wno-null-dereference>
   )
endif()

target_link_libraries(Omniscope
   PRIVATE OmniscopeCore CycloneDDSLib PubSubLib ProtoLib
)
