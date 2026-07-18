#include "CycloneDDS/CycloneRawTopicSubscriber.h"

#include "CycloneDDS/BlobSertype.h"
#include "CommonUtils/GeneralLogger.h"

#include <dds/dds.h>
#include <dds/ddsi/ddsi_serdata.h>
#include <dds/ddsi/ddsi_sertype.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace CycloneDDS
{

struct CycloneRawTopicSubscriber::ActiveSub
{
   dds_entity_t      reader{0};
   std::atomic<bool> running{true};
   std::thread       thread;

   ~ActiveSub()
   {
      running = false;
      if (thread.joinable()) thread.join();
      if (reader > 0) dds_delete(reader);
   }
};

namespace
{

void pollRawCdr(dds_entity_t reader, const std::string &topic,
                const std::string &typeName,
                const std::string &reliability,
                const std::string &durability,
                int32_t historyDepth,
                const DdsCore::RawSampleCallback &callback,
                std::atomic<bool> &running)
{
   while (running.load())
   {
      struct ddsi_serdata *buf[1] = {nullptr};
      dds_sample_info_t   si[1];

      dds_return_t n = dds_takecdr(reader, buf, 1, si, DDS_ANY_STATE);

      if (n > 0 && si[0].valid_data && buf[0] != nullptr)
      {
         uint32_t sz = 0;
         const uint8_t *data = blobSerdataBytes(buf[0], &sz);

         DdsCore::RawSample sample;
         sample.typeName = typeName;
         sample.bytes.assign(data, data + sz);
         sample.reliability = reliability;
         sample.durability = durability;
         sample.historyDepth = historyDepth;

         ddsi_serdata_unref(buf[0]);
         buf[0] = nullptr;

         callback(topic, sample);
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
   }
}

} // namespace

CycloneRawTopicSubscriber::CycloneRawTopicSubscriber(dds_entity_t participant, dds_entity_t subscriber)
   : _participant(participant)
   , _subscriber(subscriber)
{
}

CycloneRawTopicSubscriber::~CycloneRawTopicSubscriber()
{
   std::lock_guard lock(_mutex);
   _activeSubs.clear();
}

void CycloneRawTopicSubscriber::subscribe(const std::string &topic,
                                          const std::string &typeName,
                                          const std::string &reliability,
                                          const std::string &durability,
                                          int32_t historyDepth,
                                          DdsCore::RawSampleCallback callback)
{
   {
      std::lock_guard lock(_mutex);
      if (_activeSubs.count(topic))
         return; // already subscribed
   }

   // Create a blob sertype that accepts any CDR bytes without type-checking.
   // This bypasses dds_find_topic entirely — we don't need the publisher to
   // respond to type lookup requests; we just want the raw wire bytes.
   const std::string &sertypeName = typeName.empty() ? topic : typeName;
   struct ddsi_sertype *sertype = blobSertypeCreate(sertypeName.c_str());
   if (!sertype)
   {
      GPERROR("CycloneRawTopicSubscriber: out of memory creating blob sertype for '{}'", topic);
      return;
   }

   // dds_create_topic_sertype transfers ownership of sertype to the domain on
   // success (and sets sertype = nullptr). On failure, caller still owns it.
   dds_entity_t topicEntity = dds_create_topic_sertype(
      _participant, topic.c_str(), &sertype, nullptr, nullptr, nullptr);

   if (topicEntity < 0)
   {
      if (sertype) ddsi_sertype_unref(sertype);
      GPERROR("CycloneRawTopicSubscriber: failed to create topic entity for '{}' ({})", topic, topicEntity);
      return;
   }

   // Best-effort, volatile reader — passive, doesn't affect publisher flow control.
   dds_qos_t *qos = dds_create_qos();
   dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, DDS_SECS(1));
   dds_qset_durability(qos, DDS_DURABILITY_VOLATILE);
   dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

   dds_entity_t reader = dds_create_reader(_subscriber, topicEntity, qos, nullptr);
   dds_delete_qos(qos);
   dds_delete(topicEntity); // reader holds its own reference

   if (reader < 0)
   {
      GPERROR("CycloneRawTopicSubscriber: failed to create reader for '{}' ({})", topic, reader);
      return;
   }

   auto sub = std::make_unique<ActiveSub>();
   sub->reader  = reader;
   sub->running = true;
   auto *runPtr = &sub->running;

   sub->thread = std::thread(
      [reader, topic, typeName, reliability, durability, historyDepth, callback, runPtr]()
      {
         pollRawCdr(reader, topic, typeName, reliability, durability, historyDepth,
                    callback, *runPtr);
      });

   std::lock_guard lock(_mutex);
   _activeSubs[topic] = std::move(sub);

   GPINFO("CycloneRawTopicSubscriber: subscribed to '{}'", topic);
}

void CycloneRawTopicSubscriber::unsubscribe(const std::string &topic)
{
   std::lock_guard lock(_mutex);
   _activeSubs.erase(topic);
   GPINFO("CycloneRawTopicSubscriber: unsubscribed from '{}'", topic);
}

bool CycloneRawTopicSubscriber::isSubscribed(const std::string &topic) const
{
   std::lock_guard lock(_mutex);
   return _activeSubs.count(topic) > 0;
}

} // namespace CycloneDDS
