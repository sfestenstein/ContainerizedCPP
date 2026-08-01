#ifndef OBSERVABILITY_METRICSPIPELINE_H_
#define OBSERVABILITY_METRICSPIPELINE_H_

#include "Observability/ExporterProtocol.h"

// OpenTelemetry headers
#include <opentelemetry/sdk/metrics/meter_provider.h>

// System headers
#include <chrono>
#include <memory>
#include <string>

namespace Observability
{

/**
 * @brief Options controlling the OTel metrics SDK pipeline built by
 *        initMetrics().
 */
struct MetricsOptions
{
   std::string serviceName;
   std::chrono::milliseconds aggregationPeriod{5000};
   ExporterProtocol protocol{ExporterProtocol::Grpc};
   /// How long a single export can run before being cancelled. Must be less
   /// than aggregationPeriod -- see MetricsConfig.h's loadMetricsOptions()
   /// for the config-file-driven path, which validates this at load time.
   std::chrono::milliseconds exportTimeout{2500};
   /// Explicit OTLP endpoint (e.g. "http://localhost:8080/v1/metrics" for
   /// Http, or "localhost:4317" for Grpc). Empty means "let the SDK use its
   /// own default / the OTEL_EXPORTER_OTLP_ENDPOINT environment variable."
   /// Ignored for Console.
   std::string endpoint;
};

/**
 * @brief Build the OTel metrics SDK pipeline, register it globally, and
 *        install the real recorder into the metrics registry.
 *
 * Builds the exporter for options.protocol (see MetricsExporterFactory.h),
 * wraps it in a periodic reader (exporting every options.aggregationPeriod),
 * and registers the resulting
 * MeterProvider globally via opentelemetry::metrics::Provider (the one
 * legitimate global registration OTel's design requires). As its last
 * step, constructs an OtelInterfaceMetrics from this pipeline's Meter and
 * installs it via Observability::setMetrics() -- after this call returns,
 * every caller of Observability::metrics() (e.g. DDSPublisher,
 * DDSSubscriber) reaches the real recorder instead of the NoOp default.
 *
 * Call once at process startup. Returns the concrete SDK provider so
 * main() can ForceFlush() it on exit.
 *
 * @param options Service name (used as the "service.name" resource
 *                attribute) and export aggregation period.
 */
std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> initMetrics(const MetricsOptions &options);

} // namespace Observability

#endif // OBSERVABILITY_METRICSPIPELINE_H_
