#include "CycloneDDS/CycloneDiscoveryService.h"

#include <algorithm>
#include <set>

#include <format>

namespace
{

int durabilityRank(const std::string &durability)
{
   if (durability == "persistent") return 3;
   if (durability == "transient") return 2;
   if (durability == "transient_local") return 1;
   return 0;
}

std::string participantLabel(const DdsCore::DiscoveredTopic &dt)
{
   if (!dt.endpointName.empty())
      return dt.endpointName;

   if (!dt.participantId.empty())
   {
      const std::string shortId = dt.participantId.substr(0, std::min<size_t>(8, dt.participantId.size()));
      return "participant " + shortId;
   }

   if (!dt.endpointId.empty())
   {
      const std::string shortId = dt.endpointId.substr(0, std::min<size_t>(8, dt.endpointId.size()));
      return "endpoint " + shortId;
   }

   return "unknown";
}

std::string historyDepthLabel(int32_t historyDepth)
{
   if (historyDepth == -1)
      return "keep_all";
   return std::format("keep_last({})", historyDepth);
}

std::string profileKey(const DdsCore::DiscoveredTopic &dt)
{
   return dt.reliability + "|" + dt.durability + "|" + std::to_string(dt.historyDepth);
}

std::string profileLabel(const DdsCore::DiscoveredTopic &dt)
{
   return "reliability=" + dt.reliability
      + ", durability=" + dt.durability
      + ", history=" + historyDepthLabel(dt.historyDepth);
}

} // namespace

namespace CycloneDDS
{

CycloneDiscoveryService::CycloneDiscoveryService(dds_entity_t participant)
{
   _publisherReader = std::make_unique<BuiltinTopicReader>(
      participant,
      DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
      [this](const DdsCore::DiscoveredTopic &dt, bool appeared, TopicEndpointRole role,
             dds_instance_handle_t handle)
      { onDiscovery(dt, appeared, role, handle); });

   _subscriberReader = std::make_unique<BuiltinTopicReader>(
      participant,
      DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION,
      [this](const DdsCore::DiscoveredTopic &dt, bool appeared, TopicEndpointRole role,
             dds_instance_handle_t handle)
      { onDiscovery(dt, appeared, role, handle); });
}

CycloneDiscoveryService::~CycloneDiscoveryService() = default;

std::vector<std::string> CycloneDiscoveryService::topicNames() const
{
   std::lock_guard lock(_mutex);
   std::vector<std::string> names;
   names.reserve(_topics.size());
   for (const auto &[name, _] : _topics)
      names.push_back(name);
   return names;
}

std::optional<DdsCore::DiscoveredTopic>
CycloneDiscoveryService::lookup(const std::string &topic) const
{
   auto summary = summarizeTopic(topic);
   if (summary.name.empty()) return std::nullopt;
   return summary;
}

void CycloneDiscoveryService::setTopicsChangedCallback(DdsCore::TopicsChangedCallback callback)
{
   _topicsChangedCb = std::move(callback);
}

int CycloneDiscoveryService::durabilityRank(const std::string &durability)
{
   return ::durabilityRank(durability);
}

std::optional<std::string>
CycloneDiscoveryService::mismatchReason(const DdsCore::DiscoveredTopic &publisher,
                                        const DdsCore::DiscoveredTopic &subscriber)
{
   std::vector<std::string> reasons;

   if (subscriber.reliability == "reliable" && publisher.reliability == "best_effort")
   {
      reasons.push_back("reliability: publisher is best_effort, subscriber requires reliable");
   }

   if (durabilityRank(subscriber.durability) > durabilityRank(publisher.durability))
   {
      reasons.push_back("durability: publisher is less durable than subscriber requires");
   }

   if (subscriber.historyDepth == -1 && publisher.historyDepth != -1)
   {
      reasons.push_back("history: subscriber requests KEEP_ALL, publisher offers KEEP_LAST");
   }

   if (reasons.empty()) return std::nullopt;

   std::string joined = reasons.front();
   for (size_t i = 1; i < reasons.size(); ++i)
      joined += "; " + reasons[i];
   return joined;
}

void CycloneDiscoveryService::onDiscovery(const DdsCore::DiscoveredTopic &dt, bool appeared,
                                          TopicEndpointRole role,
                                          dds_instance_handle_t endpointHandle)
{
   bool topicListChanged = false;
   {
      std::lock_guard lock(_mutex);
      auto &state = _topics[dt.name];
      if (appeared)
      {
         auto &endpoints = (role == TopicEndpointRole::Publisher)
                              ? state.publishers
                              : state.subscribers;
         endpoints[endpointHandle] = EndpointSnapshot{dt};
         topicListChanged = true;
      }
      else
      {
         auto &endpoints = (role == TopicEndpointRole::Publisher)
                              ? state.publishers
                              : state.subscribers;
         endpoints.erase(endpointHandle);
         topicListChanged = true;
         if (state.publishers.empty() && state.subscribers.empty())
         {
            _topics.erase(dt.name);
         }
      }
   }

   if (topicListChanged && _topicsChangedCb)
      _topicsChangedCb();
}

DdsCore::DiscoveredTopic CycloneDiscoveryService::summarizeTopic(const std::string &topic) const
{
   std::lock_guard lock(_mutex);
   auto it = _topics.find(topic);
   if (it == _topics.end()) return {};

   DdsCore::DiscoveredTopic summary;
   summary.name = topic;

   const auto &state = it->second;
   summary.publisherCount = static_cast<int32_t>(state.publishers.size());
   summary.subscriberCount = static_cast<int32_t>(state.subscribers.size());

   const DdsCore::DiscoveredTopic *representative = nullptr;
   if (!state.publishers.empty())
      representative = &state.publishers.begin()->second.topic;
   else if (!state.subscribers.empty())
      representative = &state.subscribers.begin()->second.topic;

   if (representative)
   {
      summary.typeName = representative->typeName;
      summary.reliability = representative->reliability;
      summary.durability = representative->durability;
      summary.historyDepth = representative->historyDepth;
   }

   for (const auto &[_, pub] : state.publishers)
   {
      for (const auto &[__, sub] : state.subscribers)
      {
         if (auto reason = mismatchReason(pub.topic, sub.topic))
         {
            summary.qosMismatch = true;
            if (summary.qosMismatchReason.empty())
               summary.qosMismatchReason = *reason;
         }
      }
   }

   std::set<std::string> publisherApps;
   std::set<std::string> subscriberApps;
   std::map<std::string, std::set<std::string>> publisherProfileApps;
   std::map<std::string, std::set<std::string>> subscriberProfileApps;
   std::map<std::string, std::string> profileLabels;

   for (const auto &[_, pub] : state.publishers)
   {
      publisherApps.insert(participantLabel(pub.topic));
      const auto key = profileKey(pub.topic);
      profileLabels[key] = profileLabel(pub.topic);
      publisherProfileApps[key].insert(participantLabel(pub.topic));
   }

   for (const auto &[_, sub] : state.subscribers)
   {
      subscriberApps.insert(participantLabel(sub.topic));
      const auto key = profileKey(sub.topic);
      profileLabels[key] = profileLabel(sub.topic);
      subscriberProfileApps[key].insert(participantLabel(sub.topic));
   }

   summary.publisherApplications.assign(publisherApps.begin(), publisherApps.end());
   summary.subscriberApplications.assign(subscriberApps.begin(), subscriberApps.end());

   for (const auto &[profile, apps] : publisherProfileApps)
   {
      DdsCore::QosProfileUsage usage;
      usage.profile = profileLabels[profile];
      usage.applications.assign(apps.begin(), apps.end());
      summary.publisherQosProfiles.push_back(std::move(usage));
   }

   for (const auto &[profile, apps] : subscriberProfileApps)
   {
      DdsCore::QosProfileUsage usage;
      usage.profile = profileLabels[profile];
      usage.applications.assign(apps.begin(), apps.end());
      summary.subscriberQosProfiles.push_back(std::move(usage));
   }

   return summary;
}

} // namespace CycloneDDS
