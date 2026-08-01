# =============================================================================
# Observability Library - CMakeLists.txt
#
# Transport-agnostic interface metrics (messages/bytes sent & received per
# interface name/type/topic), backed by the OpenTelemetry C++ SDK. Transport
# libraries (CycloneDDS, future gRPC/ZMQ libs) depend on this and only ever
# see the IInterfaceMetrics abstraction (IInterfaceMetrics.h), reached via
# the swappable registry in MetricsRegistry.h.
# =============================================================================

project(Observability CXX)

find_package(opentelemetry-cpp CONFIG REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(${PROJECT_NAME} STATIC
   MetricsPipeline.cpp
   MetricsExporterFactory.cpp
   MetricsConfig.cpp
   OtelInterfaceMetrics.cpp
   ProcessSampler.cpp
   ProcessMetrics.cpp
   OtelLogSink.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Consumed by MetricsPipeline.cpp's resource-building step as the
# "service.version" resource attribute. ContainerizedCPP_VERSION comes from
# the root project()'s VERSION, not this subdirectory's own (unversioned)
# project(Observability CXX) call.
target_compile_definitions(${PROJECT_NAME} PRIVATE
   OBSERVABILITY_SERVICE_VERSION="${ContainerizedCPP_VERSION}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
   spdlog::spdlog
   yaml-cpp::yaml-cpp
   opentelemetry-cpp::metrics
   opentelemetry-cpp::otlp_grpc_metrics_exporter
   opentelemetry-cpp::otlp_http_metric_exporter
   opentelemetry-cpp::ostream_metrics_exporter
   opentelemetry-cpp::logs
   opentelemetry-cpp::otlp_grpc_log_record_exporter
)
