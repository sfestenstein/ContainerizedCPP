#ifndef OBSERVABILITY_OTELINTERFACEMETRICS_H_
#define OBSERVABILITY_OTELINTERFACEMETRICS_H_

#include "Observability/IInterfaceMetrics.h"

// OpenTelemetry headers
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/unique_ptr.h>

// System headers
#include <mutex>
#include <unordered_map>

namespace Observability
{

/**
 * @brief Concrete IInterfaceMetrics recorder backed by the OpenTelemetry
 *        metrics SDK.
 *
 * Constructed with an explicit Meter rather than looking one up via the
 * global opentelemetry::metrics::Provider on every call. All counters are
 * created once, here in the constructor, instead of via function-local-
 * static lazy lookups.
 *
 * MetricsPipeline::initMetrics() constructs one of these and installs it
 * into the MetricsRegistry (Observability::setMetrics()) as its last step
 * -- nothing else in this codebase constructs OtelInterfaceMetrics
 * directly. It lives for the process lifetime (held by the registry), so
 * passing `this` as the active-probe gauge callback's state is safe --
 * unlike a per-instance DDSSubscriber, which instead hands over a pointer
 * to its own running flag (see registerActiveProbe()).
 */
class OtelInterfaceMetrics : public IInterfaceMetrics
{
public:
   explicit OtelInterfaceMetrics(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter);
   ~OtelInterfaceMetrics() override;

   // Non-copyable, non-movable (registers `this` as an async-instrument
   // callback's state; a move would leave that pointer dangling).
   OtelInterfaceMetrics(const OtelInterfaceMetrics &) = delete;
   OtelInterfaceMetrics &operator=(const OtelInterfaceMetrics &) = delete;
   OtelInterfaceMetrics(OtelInterfaceMetrics &&) = delete;
   OtelInterfaceMetrics &operator=(OtelInterfaceMetrics &&) = delete;

   void recordSent(const std::string &interfaceName, const char *type, const std::string &topic,
                    uint64_t bytes) override;

   void recordReceived(const std::string &interfaceName, const char *type, const std::string &topic,
                        uint64_t bytes) override;

   void recordLatency(const std::string &interfaceName, const char *type, const std::string &topic,
                       std::chrono::nanoseconds latency) override;

   void registerActiveProbe(const std::string &interfaceName, const char *type,
                             const std::string &topic, const std::atomic<bool> *activeFlag) override;

   void unregisterActiveProbe(const std::atomic<bool> *activeFlag) override;

private:
   struct ProbeInfo
   {
      std::string interfaceName;
      const char *type;
      std::string topic;
   };

   static void observeActive(opentelemetry::metrics::ObserverResult result, void *state);

   opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> _messagesSent;
   opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> _messagesReceived;
   opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> _bytesSent;
   opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> _bytesReceived;
   opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>> _latencyMs;

   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _activeGauge;
   std::mutex _probesMutex;
   std::unordered_map<const std::atomic<bool> *, ProbeInfo> _probes;
};

} // namespace Observability

#endif // OBSERVABILITY_OTELINTERFACEMETRICS_H_
