#ifndef CYCLONEDDS_CYCLONERAWTOPICPUBLISHER_H_
#define CYCLONEDDS_CYCLONERAWTOPICPUBLISHER_H_

#include "DdsCore/IRawTopicPublisher.h"

#include <dds/dds.h>

#include <map>
#include <mutex>
#include <string>

struct ddsi_sertype;

namespace CycloneDDS
{

/**
 * @brief CycloneDDS implementation of DdsCore::IRawTopicPublisher.
 *
 * Publishes pre-serialized (raw CDR) samples onto arbitrary, runtime-named
 * topics using a blob sertype (see BlobSertype.h) — this is wire-level
 * replay, not general value-to-wire encoding.
 *
 * Constructed against an existing participant/publisher pair (see
 * CycloneDdsParticipant) rather than owning its own — it must share the
 * same participant as any CycloneRawTopicSubscriber used alongside it.
 * dds_create_topic_sertype() deduplicates its sertype by (participant,
 * topic-name): whichever of this class or CycloneRawTopicSubscriber
 * registers a topic first "wins" the sertype instance that the other one
 * then reuses, so both must be constructed against the same participant
 * for a given topic to stay consistent.
 */
class CycloneRawTopicPublisher : public DdsCore::IRawTopicPublisher
{
public:
   CycloneRawTopicPublisher(dds_entity_t participant, dds_entity_t publisher);
   ~CycloneRawTopicPublisher() override;

   CycloneRawTopicPublisher(const CycloneRawTopicPublisher &) = delete;
   CycloneRawTopicPublisher &operator=(const CycloneRawTopicPublisher &) = delete;

   bool publish(const std::string &topic, const DdsCore::RawSample &sample) override;

private:
   // A lazily-created replay writer for one topic. The sertype pointer is
   // the exact instance handed to dds_create_topic_sertype() for this
   // writer's topic entity — reused (not recreated) for every publish()
   // call so the serdata built for replay is guaranteed compatible with
   // the writer. Note: the writer cache is keyed by topic name only, so if
   // publish() is ever called for the same topic with different
   // typeName/QoS across calls, only the first call's metadata sticks for
   // the writer's lifetime.
   struct ActiveWriter
   {
      dds_entity_t   writer{0};
      ddsi_sertype  *sertype{nullptr};
   };

   ActiveWriter *getOrCreateWriter(const std::string &topic, const DdsCore::RawSample &sample);

   dds_entity_t _participant;
   dds_entity_t _publisher;

   mutable std::mutex _mutex;
   std::map<std::string, ActiveWriter> _activeWriters;
};

} // namespace CycloneDDS

#endif // CYCLONEDDS_CYCLONERAWTOPICPUBLISHER_H_
