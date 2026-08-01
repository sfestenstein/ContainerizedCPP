#ifndef OBSERVABILITY_METRICSEXPORTERFACTORY_H_
#define OBSERVABILITY_METRICSEXPORTERFACTORY_H_

#include "Observability/ExporterProtocol.h"

// OpenTelemetry headers
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>

// System headers
#include <memory>

namespace Observability
{

/**
 * @brief Build the metrics PushMetricExporter for the requested transport.
 *
 * Isolates transport selection from MetricsPipeline: adding a fourth
 * exporter is a new ExporterProtocol enumerator plus one branch here, not a
 * change to pipeline-construction logic -- see DESIGN.md's OCP goal.
 */
std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
createMetricExporter(ExporterProtocol protocol);

} // namespace Observability

#endif // OBSERVABILITY_METRICSEXPORTERFACTORY_H_
