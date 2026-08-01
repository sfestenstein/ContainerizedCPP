#ifndef OBSERVABILITY_NOOPINTERFACEMETRICS_H_
#define OBSERVABILITY_NOOPINTERFACEMETRICS_H_

#include "Observability/IInterfaceMetrics.h"

// System headers
#include <memory>

namespace Observability
{

/**
 * @brief Stateless Null Object implementing IInterfaceMetrics.
 *
 * This is the default value held by the MetricsRegistry (see
 * MetricsRegistry.h) until MetricsPipeline::initMetrics() swaps in the real
 * OtelInterfaceMetrics -- recording against it before then is a harmless
 * no-op rather than a null-pointer dereference or a crash.
 */
class NoOpInterfaceMetrics : public IInterfaceMetrics
{
public:
   void recordSent(const std::string & /*interfaceName*/, const char * /*type*/,
                    const std::string & /*topic*/, uint64_t /*bytes*/) override
   {
   }

   void recordReceived(const std::string & /*interfaceName*/, const char * /*type*/,
                        const std::string & /*topic*/, uint64_t /*bytes*/) override
   {
   }

   void recordLatency(const std::string & /*interfaceName*/, const char * /*type*/,
                       const std::string & /*topic*/, std::chrono::nanoseconds /*latency*/) override
   {
   }

   void registerActiveProbe(const std::string & /*interfaceName*/, const char * /*type*/,
                             const std::string & /*topic*/,
                             const std::atomic<bool> * /*activeFlag*/) override
   {
   }

   void unregisterActiveProbe(const std::atomic<bool> * /*activeFlag*/) override
   {
   }

   /**
    * @brief The single shared instance -- stateless, so sharing is safe.
    */
   static std::shared_ptr<IInterfaceMetrics> instance()
   {
      static auto shared = std::make_shared<NoOpInterfaceMetrics>();
      return shared;
   }
};

} // namespace Observability

#endif // OBSERVABILITY_NOOPINTERFACEMETRICS_H_
