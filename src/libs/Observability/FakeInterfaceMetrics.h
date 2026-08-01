#ifndef OBSERVABILITY_FAKEINTERFACEMETRICS_H_
#define OBSERVABILITY_FAKEINTERFACEMETRICS_H_

#include "Observability/IInterfaceMetrics.h"

// System headers
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace Observability
{

/**
 * @brief One recorded recordSent()/recordReceived() call.
 */
struct RecordedCall
{
   std::string interfaceName;
   std::string type;
   std::string topic;
   uint64_t bytes;
};

/**
 * @brief One recorded recordLatency() call.
 */
struct RecordedLatency
{
   std::string interfaceName;
   std::string type;
   std::string topic;
   std::chrono::nanoseconds latency;
};

/**
 * @brief Plain in-memory IInterfaceMetrics fake for tests that need real
 *        assertions on what was recorded, not just "doesn't throw."
 *
 * Deliberately not a gmock mock -- a plain recorder matches the "textbook
 * SOLID" simplicity goal of this module, even though GTest::gmock is
 * already linked into DDSTests. Install via ScopedMetrics (see
 * ScopedMetrics.h) rather than passing to a constructor: DDSPublisher/
 * DDSSubscriber reach this through the global registry, not injection.
 */
class FakeInterfaceMetrics : public IInterfaceMetrics
{
public:
   void recordSent(const std::string &interfaceName, const char *type, const std::string &topic,
                    uint64_t bytes) override
   {
      std::lock_guard<std::mutex> lock(mutex);
      sent.push_back(RecordedCall{interfaceName, type, topic, bytes});
   }

   void recordReceived(const std::string &interfaceName, const char *type, const std::string &topic,
                        uint64_t bytes) override
   {
      std::lock_guard<std::mutex> lock(mutex);
      received.push_back(RecordedCall{interfaceName, type, topic, bytes});
   }

   void recordLatency(const std::string &interfaceName, const char *type, const std::string &topic,
                       std::chrono::nanoseconds latency) override
   {
      std::lock_guard<std::mutex> lock(mutex);
      latencies.push_back(RecordedLatency{interfaceName, type, topic, latency});
   }

   void registerActiveProbe(const std::string & /*interfaceName*/, const char * /*type*/,
                             const std::string & /*topic*/,
                             const std::atomic<bool> * /*activeFlag*/) override
   {
      // Not exercised by current tests -- DDSSubscriber registers/
      // unregisters a probe on every construction/destruction, so a no-op
      // here (rather than tracking probes) keeps the fake focused on what
      // tests actually assert against.
   }

   void unregisterActiveProbe(const std::atomic<bool> * /*activeFlag*/) override
   {
   }

   mutable std::mutex mutex;
   std::vector<RecordedCall> sent;
   std::vector<RecordedCall> received;
   std::vector<RecordedLatency> latencies;
};

} // namespace Observability

#endif // OBSERVABILITY_FAKEINTERFACEMETRICS_H_
