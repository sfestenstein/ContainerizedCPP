# =============================================================================
# Observability Library - CMakeLists.txt
#
# Transport-agnostic interface metrics (messages/bytes sent & received per
# interface/type/user label set), backed by the OpenTelemetry C++ SDK. Transport
# libraries (CycloneDDS, future gRPC/ZMQ libs) depend on this and only ever
# see the IInterfaceMetrics abstraction in InterfaceMetrics.h.
# =============================================================================

project(Observability CXX)

find_package(opentelemetry-cpp CONFIG REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(${PROJECT_NAME} STATIC InterfaceMetrics.cpp MetricsConfig.cpp OtelLogSink.cpp)

set(YAML_CPP_TARGET yaml-cpp)
if(TARGET yaml-cpp::yaml-cpp)
   set(YAML_CPP_TARGET yaml-cpp::yaml-cpp)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
   spdlog::spdlog
   ${YAML_CPP_TARGET}
   opentelemetry-cpp::metrics
   opentelemetry-cpp::otlp_grpc_metrics_exporter
   opentelemetry-cpp::otlp_http_metric_exporter
   opentelemetry-cpp::ostream_metrics_exporter
   opentelemetry-cpp::logs
   opentelemetry-cpp::otlp_grpc_log_record_exporter
)
