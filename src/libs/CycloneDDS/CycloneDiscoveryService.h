#ifndef CYCLONEDDS_CYCLONEDISCOVERYSERVICE_H_
#define CYCLONEDDS_CYCLONEDISCOVERYSERVICE_H_

#include "CycloneDDS/BuiltinTopicReader.h"
#include "DdsCore/IDiscoveryService.h"

#include <dds/dds.h>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace CycloneDDS
{

/**
 * @brief CycloneDDS implementation of DdsCore::IDiscoveryService.
 *
 * Watches the DCPSPublication built-in topic (via BuiltinTopicReader) and
 * reports a topic as known only while it has at least one active
 * publisher.
 */
class CycloneDiscoveryService : public DdsCore::IDiscoveryService
{
public:
   explicit CycloneDiscoveryService(dds_entity_t participant);
   ~CycloneDiscoveryService() override;

   CycloneDiscoveryService(const CycloneDiscoveryService &) = delete;
   CycloneDiscoveryService &operator=(const CycloneDiscoveryService &) = delete;

   [[nodiscard]] std::vector<std::string> topicNames() const override;
   [[nodiscard]] std::optional<DdsCore::DiscoveredTopic>
   lookup(const std::string &topic) const override;
   void setTopicsChangedCallback(DdsCore::TopicsChangedCallback callback) override;

private:
   void onDiscovery(const DdsCore::DiscoveredTopic &dt, bool appeared,
                    dds_instance_handle_t publisherHandle);

   mutable std::mutex _mutex;
   std::map<std::string, DdsCore::DiscoveredTopic> _discoveredTopics;
   std::map<std::string, std::set<dds_instance_handle_t>> _topicPublishers;
   DdsCore::TopicsChangedCallback _topicsChangedCb;

   // Constructed last (after the members above are ready) since its
   // constructor immediately starts a thread that calls onDiscovery().
   std::unique_ptr<BuiltinTopicReader> _builtinReader;
};

} // namespace CycloneDDS

#endif // CYCLONEDDS_CYCLONEDISCOVERYSERVICE_H_
