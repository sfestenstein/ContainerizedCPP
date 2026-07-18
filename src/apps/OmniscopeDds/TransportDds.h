#ifndef TRANSPORTDDS_H_
#define TRANSPORTDDS_H_

#include "ITransport.h"

#include <cstdint>
#include <memory>

namespace Omniscope
{

/**
 * @brief DDS transport that discovers all active topics dynamically.
 *
 * Implements ITransport as a thin adapter over vendor-agnostic DdsCore
 * abstractions (IDiscoveryService, IRawTopicSubscriber, IRawTopicPublisher),
 * translating between ITransport's JSON currency and DdsCore's raw-bytes
 * currency via RawSampleJsonCodec. All CycloneDDS-specific behaviour lives
 * in the concrete Cyclone* implementations of those interfaces (see
 * src/libs/CycloneDDS/) that this class constructs and composes — swapping
 * DDS vendor means swapping which concrete implementations get constructed
 * here, not changing this class's logic.
 *
 * publishFromJson() replays previously-captured raw CDR bytes verbatim onto
 * the topic (wire-level replay), not general JSON-to-CDR encoding.
 */
class TransportDds : public ITransport
{
public:
   explicit TransportDds(uint32_t domainId);
   ~TransportDds() override;

   TransportDds(const TransportDds &) = delete;
   TransportDds &operator=(const TransportDds &) = delete;

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

#endif // TRANSPORTDDS_H_
