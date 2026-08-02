# =============================================================================
# ObservabilityDashboard — live web dashboard for OTLP/HTTP metric exports
# =============================================================================

# ---------------------------------------------------------------------------
# Embed the web UI assets (HTML/CSS/JS) as C++ raw-string includes, so the
# binary stays self-contained — no external files needed at runtime.
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/../EmbedAsset.cmake)

embed_web_asset(ObservabilityDashboardHtml web/monitor.html)
embed_web_asset(ObservabilityDashboardCss  web/style.css)
embed_web_asset(ObservabilityDashboardJs   web/app.js)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(ObservabilityDashboard
   main.cpp
   DashboardApp.cpp
)
add_dependencies(ObservabilityDashboard
   ObservabilityDashboardHtml
   ObservabilityDashboardCss
   ObservabilityDashboardJs
)

# Crow headers trigger clang-tidy false positives — disable for this target
# (matches OmniscopeDds).
set_target_properties(ObservabilityDashboard PROPERTIES CXX_CLANG_TIDY "")

target_include_directories(ObservabilityDashboard PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}   # DashboardApp.h
   ${CMAKE_CURRENT_BINARY_DIR}   # web/{monitor.html,style.css,app.js}.inc
)

target_link_libraries(ObservabilityDashboard
   PRIVATE Crow::Crow CommonUtils
)
