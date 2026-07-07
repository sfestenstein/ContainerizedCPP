#ifndef TRANSPORTDDSSNOOPER_H_
#define TRANSPORTDDSSNOOPER_H_

#include "ITransport.h"

#include <cstdint>
#include <memory>

namespace Omniscope
{

/**
 * @brief DDS transport that discovers all active topics dynamically.
 *
 * Uses the CycloneDDS C API built-in DCPSPublication topic to detect
 * publishers as they join or leave the domain. Subscribes to arbitrary
 * topics using dds_find_topic + dds_takecdr, delivering raw CDR bytes
 * (as JSON with a hex-encoded field) until Phase 2 adds Fast DDS
 * DynamicTypes decoding.
 */
class TransportDdsSnooper : public ITransport
{
public:
   explicit TransportDdsSnooper(uint32_t domainId);
   ~TransportDdsSnooper() override;

   TransportDdsSnooper(const TransportDdsSnooper &) = delete;
   TransportDdsSnooper &operator=(const TransportDdsSnooper &) = delete;

   [[nodiscard]] std::string              name() const override;
   [[nodiscard]] std::vector<std::string> topicNames() const override;

   void setTopicsChangedCallback(TopicsChangedCallback callback) override;

   void subscribe(const std::string &topic, MessageCallback callback) override;
   void unsubscribe(const std::string &topic) override;
   [[nodiscard]] bool isSubscribed(const std::string &topic) const override;

   void publishFromJson(const std::string &topic,
                        const std::string &jsonData) override;

private:
   struct Impl;
   std::unique_ptr<Impl> _impl;
};

} // namespace Omniscope

#endif // TRANSPORTDDSSNOOPER_H_
