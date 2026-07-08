# =============================================================================
# Applications - CMakeLists.txt
# =============================================================================

# Omniscope (web-based traffic inspector, supports multiple transports)
add_subdirectory(Omniscope)

# DdsSnooper (dynamic DDS topic discovery and raw message inspection)
add_subdirectory(DdsSnooper)

# RadarDDSDemo (FastDDS-specific demo applications)
add_subdirectory(RadarDDSDemo)

# Test applications (pub/sub demos, round-trip tests)
add_subdirectory(TestApps)

# Tools (benchmarks, file utilities)
add_subdirectory(Tools)
