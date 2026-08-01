#include "Observability/OtelInterfaceMetrics.h"

#include <opentelemetry/context/context.h>

#include <chrono>
#include <map>
#include <string>
#include <utility>

namespace Observability
{

namespace
{

std::map<std::string, std::string> attributes(const std::string &interfaceName, const char *type,
                                                const std::string &topic)
{
   return {
      {"interface.name", interfaceName},
      {"interface.type", type},
      {"topic", topic},
   };
}

} // namespace

OtelInterfaceMetrics::OtelInterfaceMetrics(
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter)
   : _messagesSent(
        meter->CreateUInt64Counter("interface.messages.sent", "Number of messages sent", "{message}"))
   , _messagesReceived(meter->CreateUInt64Counter("interface.messages.received",
                                                    "Number of messages received", "{message}"))
   , _bytesSent(meter->CreateUInt64Counter("interface.bytes.sent", "Number of bytes sent", "By"))
   , _bytesReceived(meter->CreateUInt64Counter("interface.bytes.received", "Number of bytes received", "By"))
   , _latencyMs(meter->CreateDoubleHistogram("interface.latency", "Send-to-receive latency", "ms"))
   , _activeGauge(meter->CreateInt64ObservableGauge(
        "interface.subscriber.active", "1 while a subscriber is running, 0 otherwise", "{subscriber}"))
{
   _activeGauge->AddCallback(&OtelInterfaceMetrics::observeActive, this);
}

OtelInterfaceMetrics::~OtelInterfaceMetrics()
{
   _activeGauge->RemoveCallback(&OtelInterfaceMetrics::observeActive, this);
}

void OtelInterfaceMetrics::recordSent(const std::string &interfaceName, const char *type,
                                       const std::string &topic, uint64_t bytes)
{
   _messagesSent->Add(1, attributes(interfaceName, type, topic));
   _bytesSent->Add(bytes, attributes(interfaceName, type, topic));
}

void OtelInterfaceMetrics::recordReceived(const std::string &interfaceName, const char *type,
                                           const std::string &topic, uint64_t bytes)
{
   _messagesReceived->Add(1, attributes(interfaceName, type, topic));
   _bytesReceived->Add(bytes, attributes(interfaceName, type, topic));
}

void OtelInterfaceMetrics::recordLatency(const std::string &interfaceName, const char *type,
                                          const std::string &topic, std::chrono::nanoseconds latency)
{
   double milliseconds = std::chrono::duration<double, std::milli>(latency).count();
   // Histogram::Record(value, attributes) with no explicit Context is only
   // declared for OPENTELEMETRY_ABI_VERSION_NO >= 2; this build uses ABI v1,
   // so an explicit empty Context is required here (unlike Counter::Add,
   // whose 2-arg overload isn't ABI-gated).
   _latencyMs->Record(milliseconds, attributes(interfaceName, type, topic), opentelemetry::context::Context{});
}

void OtelInterfaceMetrics::registerActiveProbe(const std::string &interfaceName, const char *type,
                                                const std::string &topic,
                                                const std::atomic<bool> *activeFlag)
{
   std::lock_guard<std::mutex> lock(_probesMutex);
   _probes[activeFlag] = ProbeInfo{interfaceName, type, topic};
}

void OtelInterfaceMetrics::unregisterActiveProbe(const std::atomic<bool> *activeFlag)
{
   std::lock_guard<std::mutex> lock(_probesMutex);
   _probes.erase(activeFlag);
}

void OtelInterfaceMetrics::observeActive(opentelemetry::metrics::ObserverResult result, void *state)
{
   auto *self = static_cast<OtelInterfaceMetrics *>(state);
   auto observer = opentelemetry::nostd::get<
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(result);

   std::lock_guard<std::mutex> lock(self->_probesMutex);
   for (const auto &entry : self->_probes)
   {
      const auto *activeFlag = entry.first;
      const auto &info = entry.second;
      observer->Observe(activeFlag->load() ? 1 : 0, attributes(info.interfaceName, info.type, info.topic));
   }
}

} // namespace Observability
