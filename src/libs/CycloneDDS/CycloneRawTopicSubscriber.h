#ifndef CYCLONEDDS_CYCLONERAWTOPICSUBSCRIBER_H_
#define CYCLONEDDS_CYCLONERAWTOPICSUBSCRIBER_H_

#include "DdsCore/IRawTopicSubscriber.h"

#include <dds/dds.h>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace CycloneDDS
{

/**
 * @brief CycloneDDS implementation of DdsCore::IRawTopicSubscriber.
 *
 * Subscribes to arbitrary topics using a raw-CDR blob sertype (see
 * BlobSertype.h) and dds_takecdr(), delivering samples as opaque bytes —
 * no compiled-in IDL type is required. One background thread polls per
 * active subscription.
 *
 * Constructed against an existing participant/subscriber pair (see
 * CycloneDdsParticipant) rather than owning its own — it must share the
 * same participant as any CycloneRawTopicPublisher used alongside it, so
 * that dds_create_topic_sertype()'s per-(participant, topic-name) sertype
 * deduplication keeps reader and writer sertype instances consistent for
 * the same topic (see the comment in CycloneRawTopicPublisher.h).
 */
class CycloneRawTopicSubscriber : public DdsCore::IRawTopicSubscriber
{
public:
   CycloneRawTopicSubscriber(dds_entity_t participant, dds_entity_t subscriber);
   ~CycloneRawTopicSubscriber() override;

   CycloneRawTopicSubscriber(const CycloneRawTopicSubscriber &) = delete;
   CycloneRawTopicSubscriber &operator=(const CycloneRawTopicSubscriber &) = delete;

   void subscribe(const std::string &topic,
                  const std::string &typeName,
                  const std::string &reliability,
                  const std::string &durability,
                  int32_t historyDepth,
                  DdsCore::RawSampleCallback callback) override;
   void unsubscribe(const std::string &topic) override;
   [[nodiscard]] bool isSubscribed(const std::string &topic) const override;

private:
   struct ActiveSub;

   dds_entity_t _participant;
   dds_entity_t _subscriber;

   mutable std::mutex _mutex;
   std::map<std::string, std::unique_ptr<ActiveSub>> _activeSubs;
};

} // namespace CycloneDDS

#endif // CYCLONEDDS_CYCLONERAWTOPICSUBSCRIBER_H_
