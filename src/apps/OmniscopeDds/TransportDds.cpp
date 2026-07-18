#include "TransportDds.h"

#include "CycloneDDS/CycloneDdsParticipant.h"
#include "CycloneDDS/CycloneDiscoveryService.h"
#include "CycloneDDS/CycloneRawTopicPublisher.h"
#include "CycloneDDS/CycloneRawTopicSubscriber.h"
#include "DdsCore/IDiscoveryService.h"
#include "DdsCore/IRawTopicPublisher.h"
#include "DdsCore/IRawTopicSubscriber.h"
#include "RawSampleJsonCodec.h"
#include "CommonUtils/GeneralLogger.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Omniscope
{

// ---------------------------------------------------------------------------
//  TransportDds
// ---------------------------------------------------------------------------

struct TransportDds::Impl
{
   explicit Impl(uint32_t domainId) : participant(domainId) {}

   CycloneDDS::CycloneDdsParticipant participant;

   // Dynamic topic discovery, delegated to a DdsCore::IDiscoveryService.
   std::unique_ptr<DdsCore::IDiscoveryService> discovery;

   // Raw-sample subscriptions, delegated to a DdsCore::IRawTopicSubscriber.
   // Raw-sample replay publishing, delegated to a DdsCore::IRawTopicPublisher.
   // Both declared after `participant`/`discovery` so they are destroyed
   // first, stopping their threads/DDS entities before the participant
   // goes away.
   std::unique_ptr<DdsCore::IRawTopicSubscriber> rawSubscriber;
   std::unique_ptr<DdsCore::IRawTopicPublisher> rawPublisher;
};

TransportDds::TransportDds(uint32_t domainId)
   : _impl(std::make_unique<Impl>(domainId))
{
   if (!_impl->participant.isValid())
   {
      return;
   }

   _impl->discovery = std::make_unique<CycloneDDS::CycloneDiscoveryService>(
      _impl->participant.participant());
   _impl->rawSubscriber = std::make_unique<CycloneDDS::CycloneRawTopicSubscriber>(
      _impl->participant.participant(), _impl->participant.subscriber());
   _impl->rawPublisher = std::make_unique<CycloneDDS::CycloneRawTopicPublisher>(
      _impl->participant.participant(), _impl->participant.publisher());

   GPINFO("OmniscopeDds: listening on domain {}", domainId);
}

TransportDds::~TransportDds() = default;

std::string TransportDds::name() const { return "DDS"; }

std::vector<std::string> TransportDds::topicNames() const
{
   return _impl->discovery->topicNames();
}

void TransportDds::setTopicsChangedCallback(TopicsChangedCallback callback)
{
   _impl->discovery->setTopicsChangedCallback(std::move(callback));
}

void TransportDds::subscribe(const std::string &topic, MessageCallback callback)
{
   // Retrieve the type name/QoS for the JSON envelope.
   std::string typeName;
   std::string reliability = "reliable";
   std::string durability = "volatile";
   int32_t historyDepth = 1;
   if (auto dt = _impl->discovery->lookup(topic))
   {
      typeName = dt->typeName;
      reliability = dt->reliability;
      durability = dt->durability;
      historyDepth = dt->historyDepth;
   }

   _impl->rawSubscriber->subscribe(
      topic, typeName, reliability, durability, historyDepth,
      [callback](const std::string &t, const DdsCore::RawSample &sample)
      { callback(t, RawSampleJsonCodec::toJson(sample)); });
}

void TransportDds::unsubscribe(const std::string &topic)
{
   _impl->rawSubscriber->unsubscribe(topic);
}

bool TransportDds::isSubscribed(const std::string &topic) const
{
   return _impl->rawSubscriber->isSubscribed(topic);
}

void TransportDds::publishFromJson(const std::string &topic,
                                    const std::string &jsonData)
{
   auto sample = RawSampleJsonCodec::fromJson(jsonData);
   if (!sample)
   {
      GPERROR("OmniscopeDds: publishFromJson('{}') - invalid or malformed JSON", topic);
      return;
   }

   _impl->rawPublisher->publish(topic, *sample);
}

} // namespace Omniscope
