#include "CycloneDDS/CycloneDiscoveryService.h"

namespace CycloneDDS
{

CycloneDiscoveryService::CycloneDiscoveryService(dds_entity_t participant)
{
   _builtinReader = std::make_unique<BuiltinTopicReader>(
      participant,
      [this](const DdsCore::DiscoveredTopic &dt, bool appeared,
             dds_instance_handle_t handle)
      { onDiscovery(dt, appeared, handle); });
}

CycloneDiscoveryService::~CycloneDiscoveryService() = default;

std::vector<std::string> CycloneDiscoveryService::topicNames() const
{
   std::lock_guard lock(_mutex);
   std::vector<std::string> names;
   names.reserve(_discoveredTopics.size());
   for (const auto &[name, _] : _discoveredTopics)
      names.push_back(name);
   return names;
}

std::optional<DdsCore::DiscoveredTopic>
CycloneDiscoveryService::lookup(const std::string &topic) const
{
   std::lock_guard lock(_mutex);
   auto it = _discoveredTopics.find(topic);
   if (it == _discoveredTopics.end()) return std::nullopt;
   return it->second;
}

void CycloneDiscoveryService::setTopicsChangedCallback(DdsCore::TopicsChangedCallback callback)
{
   _topicsChangedCb = std::move(callback);
}

void CycloneDiscoveryService::onDiscovery(const DdsCore::DiscoveredTopic &dt, bool appeared,
                                          dds_instance_handle_t publisherHandle)
{
   bool listChanged = false;
   {
      std::lock_guard lock(_mutex);
      auto &publishers = _topicPublishers[dt.name];
      if (appeared)
      {
         publishers.insert(publisherHandle);
         _discoveredTopics[dt.name] = dt;
         listChanged = (publishers.size() == 1); // first publisher for this topic
      }
      else
      {
         publishers.erase(publisherHandle);
         if (publishers.empty())
         {
            _discoveredTopics.erase(dt.name);
            listChanged = true;
         }
      }
   }

   if (listChanged && _topicsChangedCb)
      _topicsChangedCb();
}

} // namespace CycloneDDS
