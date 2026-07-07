#include "TransportDdsSnooper.h"

#include "CycloneDDS/BlobSertype.h"
#include "CycloneDDS/BuiltinTopicReader.h"
#include "CommonUtils/GeneralLogger.h"

#include <dds/dds.h>
#include <dds/ddsc/dds_public_impl.h>
#include <dds/ddsi/ddsi_serdata.h>
#include <dds/ddsi/ddsi_sertype.h>

#include <atomic>
#include <chrono>
#include <format>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace Omniscope
{

// ---------------------------------------------------------------------------
//  Internal state
// ---------------------------------------------------------------------------

struct ActiveSub
{
   dds_entity_t     reader{0};
   std::atomic<bool> running{true};
   std::thread      thread;

   ~ActiveSub()
   {
      running = false;
      if (thread.joinable()) thread.join();
      if (reader > 0) dds_delete(reader);
   }
};

struct TransportDdsSnooper::Impl
{
   dds_entity_t participant{0};
   dds_entity_t subscriber{0};

   // Discovery: topic name → last-seen info; GUID set tracks per-topic publisher count.
   std::map<std::string, CycloneDDS::DiscoveredTopic> discoveredTopics;
   std::map<std::string, std::set<dds_instance_handle_t>> topicPublishers;

   // Active data subscriptions
   std::map<std::string, std::unique_ptr<ActiveSub>> activeSubs;

   mutable std::mutex mutex;

   TopicsChangedCallback topicsChangedCb;

   std::unique_ptr<CycloneDDS::BuiltinTopicReader> builtinReader;

   // Called from the BuiltinTopicReader polling thread.
   void onDiscovery(const CycloneDDS::DiscoveredTopic &dt, bool appeared,
                    dds_instance_handle_t publisherHandle)
   {
      bool listChanged = false;
      {
         std::lock_guard lock(mutex);
         auto &publishers = topicPublishers[dt.name];
         if (appeared)
         {
            publishers.insert(publisherHandle);
            discoveredTopics[dt.name] = dt;
            listChanged = (publishers.size() == 1); // first publisher for this topic
         }
         else
         {
            publishers.erase(publisherHandle);
            if (publishers.empty())
            {
               discoveredTopics.erase(dt.name);
               listChanged = true;
            }
         }
      }

      if (listChanged && topicsChangedCb)
         topicsChangedCb();
   }
};

// ---------------------------------------------------------------------------
//  Raw CDR polling helper
// ---------------------------------------------------------------------------

static void pollRawCdr(dds_entity_t reader, const std::string &topic,
                       const std::string &typeName,
                       const MessageCallback &callback,
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

         std::string hex;
         hex.reserve(sz * 2);
         constexpr char kHex[] = "0123456789abcdef";
         for (uint32_t i = 0; i < sz; ++i)
         {
            hex += kHex[data[i] >> 4];
            hex += kHex[data[i] & 0xf];
         }

         ddsi_serdata_unref(buf[0]);
         buf[0] = nullptr;

         std::string json = std::format(
            R"({{"raw_cdr":"{}","byte_count":{},"type_name":"{}"}})",
            hex, sz, typeName);

         callback(topic, json);
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
   }
}

// ---------------------------------------------------------------------------
//  TransportDdsSnooper
// ---------------------------------------------------------------------------

TransportDdsSnooper::TransportDdsSnooper(uint32_t domainId)
   : _impl(std::make_unique<Impl>())
{
   _impl->participant = dds_create_participant(
      static_cast<dds_domainid_t>(domainId), nullptr, nullptr);

   if (_impl->participant < 0)
   {
      GPERROR("DdsSnooper: failed to create participant on domain {} ({})",
              domainId, _impl->participant);
      return;
   }

   _impl->subscriber = dds_create_subscriber(_impl->participant, nullptr, nullptr);

   _impl->builtinReader = std::make_unique<CycloneDDS::BuiltinTopicReader>(
      _impl->participant,
      [this](const CycloneDDS::DiscoveredTopic &dt, bool appeared,
             dds_instance_handle_t handle)
      { _impl->onDiscovery(dt, appeared, handle); });

   GPINFO("DdsSnooper: listening on domain {}", domainId);
}

TransportDdsSnooper::~TransportDdsSnooper()
{
   // Stop subscriptions before tearing down DDS entities.
   {
      std::lock_guard lock(_impl->mutex);
      _impl->activeSubs.clear();
   }
   _impl->builtinReader.reset();

   if (_impl->subscriber > 0) dds_delete(_impl->subscriber);
   if (_impl->participant > 0) dds_delete(_impl->participant);
}

std::string TransportDdsSnooper::name() const { return "DDS Snooper"; }

std::vector<std::string> TransportDdsSnooper::topicNames() const
{
   std::lock_guard lock(_impl->mutex);
   std::vector<std::string> names;
   names.reserve(_impl->discoveredTopics.size());
   for (const auto &[name, _] : _impl->discoveredTopics)
      names.push_back(name);
   return names;
}

void TransportDdsSnooper::setTopicsChangedCallback(TopicsChangedCallback callback)
{
   _impl->topicsChangedCb = std::move(callback);
}

void TransportDdsSnooper::subscribe(const std::string &topic, MessageCallback callback)
{
   {
      std::lock_guard lock(_impl->mutex);
      if (_impl->activeSubs.count(topic))
         return; // already subscribed
   }

   // Retrieve the type name for the JSON envelope.
   std::string typeName;
   {
      std::lock_guard lock(_impl->mutex);
      auto it = _impl->discoveredTopics.find(topic);
      if (it != _impl->discoveredTopics.end())
         typeName = it->second.typeName;
   }

   // Create a blob sertype that accepts any CDR bytes without type-checking.
   // This bypasses dds_find_topic entirely — we don't need the publisher to
   // respond to type lookup requests; we just want the raw wire bytes.
   const char *sertypeName = typeName.empty() ? topic.c_str() : typeName.c_str();
   struct ddsi_sertype *sertype = blobSertypeCreate(sertypeName);
   if (!sertype)
   {
      GPERROR("DdsSnooper: out of memory creating blob sertype for '{}'", topic);
      return;
   }

   // dds_create_topic_sertype transfers ownership of sertype to the domain on
   // success (and sets sertype = nullptr). On failure, caller still owns it.
   dds_entity_t topicEntity = dds_create_topic_sertype(
      _impl->participant, topic.c_str(), &sertype, nullptr, nullptr, nullptr);

   if (topicEntity < 0)
   {
      if (sertype) ddsi_sertype_unref(sertype);
      GPERROR("DdsSnooper: failed to create topic entity for '{}' ({})", topic, topicEntity);
      return;
   }

   // Best-effort, volatile reader — passive, doesn't affect publisher flow control.
   dds_qos_t *qos = dds_create_qos();
   dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, DDS_SECS(1));
   dds_qset_durability(qos, DDS_DURABILITY_VOLATILE);
   dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

   dds_entity_t reader = dds_create_reader(_impl->subscriber, topicEntity, qos, nullptr);
   dds_delete_qos(qos);
   dds_delete(topicEntity); // reader holds its own reference

   if (reader < 0)
   {
      GPERROR("DdsSnooper: failed to create reader for '{}' ({})", topic, reader);
      return;
   }

   auto sub = std::make_unique<ActiveSub>();
   sub->reader  = reader;
   sub->running = true;
   auto *runPtr = &sub->running;

   sub->thread = std::thread(
      [reader, topic, typeName, callback, runPtr]()
      { pollRawCdr(reader, topic, typeName, callback, *runPtr); });

   std::lock_guard lock(_impl->mutex);
   _impl->activeSubs[topic] = std::move(sub);

   GPINFO("DdsSnooper: subscribed to '{}'", topic);
}

void TransportDdsSnooper::unsubscribe(const std::string &topic)
{
   std::lock_guard lock(_impl->mutex);
   _impl->activeSubs.erase(topic);
   GPINFO("DdsSnooper: unsubscribed from '{}'", topic);
}

bool TransportDdsSnooper::isSubscribed(const std::string &topic) const
{
   std::lock_guard lock(_impl->mutex);
   return _impl->activeSubs.count(topic) > 0;
}

void TransportDdsSnooper::publishFromJson(const std::string &topic,
                                          const std::string & /*jsonData*/)
{
   GPWARN("DdsSnooper: publishFromJson on '{}' not supported in Phase 1 "
          "(no type information for CDR encoding)", topic);
}

} // namespace Omniscope
