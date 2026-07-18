# =============================================================================
# OmniscopeDds — web-based DDS traffic inspector with dynamic topic discovery
# =============================================================================

# ---------------------------------------------------------------------------
# Embed the web UI assets (HTML/CSS/JS) as C++ raw-string includes, so the
# binary stays self-contained — no external files needed at runtime.
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/../EmbedAsset.cmake)

embed_web_asset(OmniscopeDdsHtml web/monitor.html)
embed_web_asset(OmniscopeDdsCss  web/style.css)
embed_web_asset(OmniscopeDdsJs   web/app.js)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(OmniscopeDds
   main.cpp
   OmniscopeApp.cpp
   PlaybackEngine.cpp
   RawSampleJsonCodec.cpp
   TransportDds.cpp
)
add_dependencies(OmniscopeDds OmniscopeDdsHtml OmniscopeDdsCss OmniscopeDdsJs)

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
   ${CMAKE_CURRENT_BINARY_DIR}   # web/{monitor.html,style.css,app.js}.inc
)

target_link_libraries(OmniscopeDds
   PRIVATE Crow::Crow CommonUtils CycloneDDSLib DdsCore
)
