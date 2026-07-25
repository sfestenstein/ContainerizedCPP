# =============================================================================
# Applications - CMakeLists.txt
# =============================================================================

# OmniscopeDds (web-based DDS traffic inspector with dynamic topic discovery)
add_subdirectory(OmniscopeDds)

# RadarDDSDemo (CycloneDDS QoS-profile demo applications)
add_subdirectory(RadarDDSDemo)

# GrpcChatLogger (minimal gRPC pub/poll demo: server + reporter + reader clients)
add_subdirectory(GrpcChatLogger)
