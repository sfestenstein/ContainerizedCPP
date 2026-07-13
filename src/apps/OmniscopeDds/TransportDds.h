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
 * Uses the CycloneDDS C API built-in DCPSPublication topic to detect
 * publishers as they join or leave the domain. Subscribes to arbitrary
 * topics using a raw-CDR blob sertype and dds_takecdr, delivering the raw
 * bytes as JSON with a hex-encoded field. publishFromJson() replays
 * previously-captured raw CDR bytes verbatim onto the topic (wire-level
 * replay), not general JSON-to-CDR encoding.
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
