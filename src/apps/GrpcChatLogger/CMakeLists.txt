# =============================================================================
# GrpcChatLogger - CMakeLists.txt
#
# Minimal gRPC demo: one server (keeps the latest 100 reported chat
# messages), one client that periodically reports new text via RPC, and a
# reader client (run as two separate processes) that periodically polls for
# and logs the latest message. Exercises the same transport-agnostic
# Observability::metrics() API RadarDDSDemo uses for DDS, this time tagged
# Observability::InterfaceType::GRPC.
# =============================================================================

project(GrpcChatLogger CXX)

include(${CMAKE_SOURCE_DIR}/cmake/GrpcGenerate.cmake)

add_grpc_library(ChatLoggerProto PROTO ${CMAKE_CURRENT_SOURCE_DIR}/chat_logger.proto)

add_library(ChatMessageStore STATIC ChatMessageStore.cpp)
target_include_directories(ChatMessageStore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# gRPC's built-in OpenTelemetry metrics plugin (grpc::OpenTelemetryPluginBuilder,
# <grpcpp/ext/otel_plugin.h>) -- reports standard RPC-layer metrics through
# the same MeterProvider Observability::init() registers. Only available on
# a gRPC build configured with -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=ON (see Dockerfile).
add_executable(GrpcChatLoggerServer ChatLoggerServer.cpp)
target_link_libraries(GrpcChatLoggerServer PRIVATE
   ChatLoggerProto
   ChatMessageStore
   CommonUtils
   Observability
   gRPC::grpcpp_otel_plugin
)

add_executable(GrpcChatLoggerReporter ChatLoggerReporter.cpp)
target_link_libraries(GrpcChatLoggerReporter PRIVATE
   ChatLoggerProto
   CommonUtils
   Observability
   gRPC::grpcpp_otel_plugin
)

add_executable(GrpcChatLoggerReader ChatLoggerReader.cpp)
target_link_libraries(GrpcChatLoggerReader PRIVATE
   ChatLoggerProto
   CommonUtils
   Observability
   gRPC::grpcpp_otel_plugin
)
