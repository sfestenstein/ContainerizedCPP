#ifndef OBSERVABILITY_PROCESSMETRICS_H_
#define OBSERVABILITY_PROCESSMETRICS_H_

#include "Observability/ProcessSampler.h"

// OpenTelemetry headers
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/nostd/shared_ptr.h>

namespace Observability
{

/**
 * @brief Observable-gauge wiring around ProcessSampler.
 *
 * A sibling of OtelInterfaceMetrics under the same Meter, not a subtype of
 * IInterfaceMetrics -- process resource usage and interface traffic are
 * separate concerns (SRP) that happen to both be implemented as OTel
 * instruments. Unlike interface.subscriber.active's multi-instance probe
 * registry (Phase C), there's exactly one process, so each gauge's callback
 * samples and reports directly with no shared registry needed.
 *
 * Constructed once by the composition root (Radar.cpp/Workstation.cpp) and
 * kept alive for the process lifetime, passing `this` as each gauge
 * callback's state -- safe because this object outlives metric collection.
 */
class ProcessMetrics
{
public:
   explicit ProcessMetrics(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter);
   ~ProcessMetrics();

   // Non-copyable, non-movable (registers `this` as async-instrument
   // callback state; a move would leave that pointer dangling).
   ProcessMetrics(const ProcessMetrics &) = delete;
   ProcessMetrics &operator=(const ProcessMetrics &) = delete;
   ProcessMetrics(ProcessMetrics &&) = delete;
   ProcessMetrics &operator=(ProcessMetrics &&) = delete;

private:
   static void observeCpu(opentelemetry::metrics::ObserverResult result, void *state);
   static void observeMemory(opentelemetry::metrics::ObserverResult result, void *state);

   ProcessSampler _sampler;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _cpuGauge;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _memoryGauge;
};

} // namespace Observability

#endif // OBSERVABILITY_PROCESSMETRICS_H_
