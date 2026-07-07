# =============================================================================
# Tools - CMakeLists.txt
# =============================================================================

# VITA 49.2 Performance Benchmark
add_executable(Vita49PerfBenchmark Vita49PerfBenchmark.cpp)

target_link_libraries(Vita49PerfBenchmark
   PRIVATE Vita49_2 CommonUtils
)

# VITA 49.2 File Codec (generate / inspect / roundtrip)
add_executable(Vita49FileCodec Vita49FileCodec.cpp)

target_link_libraries(Vita49FileCodec
   PRIVATE Vita49_2 CommonUtils
)
