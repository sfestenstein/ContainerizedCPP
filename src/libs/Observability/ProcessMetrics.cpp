#include "Observability/ProcessMetrics.h"

namespace Observability
{

ProcessMetrics::ProcessMetrics(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter)
   : _cpuGauge(meter->CreateDoubleObservableGauge(
        "process.cpu.utilization", "Fraction of one CPU core used since the previous sample", "1"))
   , _memoryGauge(
        meter->CreateInt64ObservableGauge("process.memory.usage", "Resident set size (RSS)", "By"))
{
   _cpuGauge->AddCallback(&ProcessMetrics::observeCpu, this);
   _memoryGauge->AddCallback(&ProcessMetrics::observeMemory, this);
}

ProcessMetrics::~ProcessMetrics()
{
   _cpuGauge->RemoveCallback(&ProcessMetrics::observeCpu, this);
   _memoryGauge->RemoveCallback(&ProcessMetrics::observeMemory, this);
}

void ProcessMetrics::observeCpu(opentelemetry::metrics::ObserverResult result, void *state)
{
   auto *self = static_cast<ProcessMetrics *>(state);
   auto observer = opentelemetry::nostd::get<
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(result);
   observer->Observe(self->_sampler.sample().cpuUtilization);
}

void ProcessMetrics::observeMemory(opentelemetry::metrics::ObserverResult result, void *state)
{
   auto *self = static_cast<ProcessMetrics *>(state);
   auto observer = opentelemetry::nostd::get<
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(result);
   observer->Observe(self->_sampler.sample().residentMemoryBytes);
}

} // namespace Observability
