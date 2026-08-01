#ifndef OBSERVABILITY_METRICSCONFIG_H_
#define OBSERVABILITY_METRICSCONFIG_H_

#include "Observability/MetricsPipeline.h"

// System headers
#include <string>

namespace Observability
{

/**
 * @brief Load MetricsOptions from a YAML file.
 *
 * Expected shape:
 * @code
 * observability:
 *   aggregationPeriodMs: 5000
 *   exportTimeoutMs: 2500
 *   protocol: http        # grpc | http | console
 *   endpoint: "http://localhost:8080/v1/metrics"
 * @endcode
 * All keys are optional and fall back to MetricsOptions' own defaults
 * except `protocol`, which must be one of grpc/http/console if present.
 *
 * `serviceName` is deliberately not part of this file -- it's per-app
 * identity, not an operational knob, so callers set it on the returned
 * MetricsOptions themselves:
 * @code
 * auto options = Observability::loadMetricsOptions(path);
 * options.serviceName = "RadarDDSRadar";
 * auto meterProvider = Observability::initMetrics(options);
 * @endcode
 *
 * @throws std::runtime_error if the file doesn't exist, isn't valid YAML,
 *         `protocol` isn't grpc/http/console, or `exportTimeoutMs` is not
 *         strictly less than `aggregationPeriodMs`.
 */
MetricsOptions loadMetricsOptions(const std::string &yamlPath);

} // namespace Observability

#endif // OBSERVABILITY_METRICSCONFIG_H_
