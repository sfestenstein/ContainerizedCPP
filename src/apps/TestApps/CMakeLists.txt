# =============================================================================
# Test Applications - CMakeLists.txt
# =============================================================================

# Zyre Publisher executable
add_executable(ZyrePublisher ZyrePublisherTest.cpp)

target_link_libraries(ZyrePublisher
   PRIVATE PubSubLib ProtoLib
)

# Zyre Subscriber executable
add_executable(ZyreSubscriber ZyreSubscriberTest.cpp)

target_link_libraries(ZyreSubscriber
   PRIVATE PubSubLib ProtoLib
)

# High-Bandwidth Subscriber executable
add_executable(HighBandwidthSubscriber HighBandwidthSubscriberTester.cpp)

target_link_libraries(HighBandwidthSubscriber
   PRIVATE PubSubLib ProtoLib
)

# High-Bandwidth Publisher executable
add_executable(HighBandwidthPublisher HighBandwidthPublisherTester.cpp)

target_link_libraries(HighBandwidthPublisher
   PRIVATE PubSubLib ProtoLib
)

# DDS Publisher Test
add_executable(DDSPublisher DDSPublisherTest.cpp)

target_link_libraries(DDSPublisher
   PRIVATE CycloneDDSLib CommonUtils
)

# DDS Subscriber Test
add_executable(DDSSubscriber DDSSubscriberTest.cpp)

target_link_libraries(DDSSubscriber
   PRIVATE CycloneDDSLib CommonUtils
)

# VITA 49.2 Round-Trip Test
add_executable(Vita49RoundTripTest Vita49RoundTripTest.cpp)

target_link_libraries(Vita49RoundTripTest
   PRIVATE Vita49_2 CommonUtils
)

# gRPC Server Test
add_executable(GrpcServer GrpcServerTest.cpp)

target_link_libraries(GrpcServer
   PRIVATE GrpcServiceLib CommonUtils
)

# gRPC Client Test
add_executable(GrpcClient GrpcClientTest.cpp)

target_link_libraries(GrpcClient
   PRIVATE GrpcServiceLib CommonUtils
)


