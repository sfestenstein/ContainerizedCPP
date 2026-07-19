# =============================================================================
# Observability Library - CMakeLists.txt
#
# Transport-agnostic interface metrics (messages/bytes sent & received per
# interface name/type/topic), backed by the OpenTelemetry C++ SDK. Transport
# libraries (CycloneDDS, future gRPC/ZMQ libs) depend on this and only ever
# see the IInterfaceMetrics abstraction in InterfaceMetrics.h.
# =============================================================================

project(Observability CXX)

find_package(opentelemetry-cpp CONFIG REQUIRED)
find_package(spdlog REQUIRED)

add_library(${PROJECT_NAME} STATIC InterfaceMetrics.cpp OtelLogSink.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
   spdlog::spdlog
   opentelemetry-cpp::metrics
   opentelemetry-cpp::otlp_grpc_metrics_exporter
   opentelemetry-cpp::logs
   opentelemetry-cpp::otlp_grpc_log_record_exporter
)
