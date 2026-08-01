#ifndef OBSERVABILITY_METRICSEXPORTERFACTORY_H_
#define OBSERVABILITY_METRICSEXPORTERFACTORY_H_

#include "Observability/ExporterProtocol.h"

// OpenTelemetry headers
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>

// System headers
#include <memory>
#include <string>

namespace Observability
{

/**
 * @brief Build the metrics PushMetricExporter for the requested transport.
 *
 * Isolates transport selection from MetricsPipeline: adding a fourth
 * exporter is a new ExporterProtocol enumerator plus one branch here, not a
 * change to pipeline-construction logic -- see DESIGN.md's OCP goal.
 *
 * @param protocol Which transport to build.
 * @param endpoint Explicit endpoint to export to; empty means "use the
 *                 SDK's own default / OTEL_EXPORTER_OTLP_ENDPOINT env var."
 *                 Ignored for Console (nowhere to send it).
 */
std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
createMetricExporter(ExporterProtocol protocol, const std::string &endpoint = "");

} // namespace Observability

#endif // OBSERVABILITY_METRICSEXPORTERFACTORY_H_
