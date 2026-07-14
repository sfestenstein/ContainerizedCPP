#include "TransportDds.h"

#include "CycloneDDS/BlobSertype.h"
#include "CycloneDDS/BuiltinTopicReader.h"
#include "CommonUtils/GeneralLogger.h"

#include <crow/json.h>

#include <dds/dds.h>
#include <dds/ddsc/dds_public_impl.h>
#include <dds/ddsi/ddsi_serdata.h>
#include <dds/ddsi/ddsi_sertype.h>

#include <atomic>
#include <chrono>
#include <cstdint>
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

// A lazily-created replay writer for one topic. The sertype pointer is the
// exact instance handed to dds_create_topic_sertype() for this writer's
// topic entity — reused (not recreated) for every publishFromJson() call so
// the serdata built for replay is guaranteed compatible with the writer.
struct ActiveWriter
{
   dds_entity_t writer{0};
   struct ddsi_sertype *sertype{nullptr};
};

struct TransportDds::Impl
{
   dds_entity_t participant{0};
   dds_entity_t subscriber{0};
   dds_entity_t publisher{0};

   // Discovery: topic name → last-seen info; GUID set tracks per-topic publisher count.
   std::map<std::string, CycloneDDS::DiscoveredTopic> discoveredTopics;
   std::map<std::string, std::set<dds_instance_handle_t>> topicPublishers;

   // Active data subscriptions
   std::map<std::string, std::unique_ptr<ActiveSub>> activeSubs;

   // Lazily-created replay writers, one per topic ever played back to.
   std::map<std::string, ActiveWriter> activeWriters;

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

   // Lazily creates (or returns the existing) replay writer for a topic,
   // mirroring subscribe()'s lazy-reader creation below.
   ActiveWriter *getOrCreateWriter(const std::string &topic,
                                   const std::string &typeName,
                                   const std::string &reliability,
                                   const std::string &durability,
                                   int32_t historyDepth)
   {
      {
         std::lock_guard lock(mutex);
         auto it = activeWriters.find(topic);
         if (it != activeWriters.end()) return &it->second;
      }

      const std::string &sertypeName = typeName.empty() ? topic : typeName;
      struct ddsi_sertype *sertype = blobSertypeCreate(sertypeName.c_str());
      if (!sertype)
      {
         GPERROR("OmniscopeDds: out of memory creating blob sertype for writer '{}'", topic);
         return nullptr;
      }

      // dds_create_topic_sertype takes ownership of *sertype on success and
      // writes back, in the same out-param, a pointer to whichever sertype
      // is actually bound to the topic: our freshly-created one if this
      // topic name wasn't registered yet, or a pre-existing one (freeing
      // ours) if a reader/writer already registered this topic name earlier
      // — which is the common case here, since subscribe() may already have
      // created a reader for this exact topic. Only read the pointer back
      // *after* the call succeeds; never keep the pre-call value.
      dds_entity_t topicEntity = dds_create_topic_sertype(
         participant, topic.c_str(), &sertype, nullptr, nullptr, nullptr);

      if (topicEntity < 0)
      {
         if (sertype) ddsi_sertype_unref(sertype);
         GPERROR("OmniscopeDds: failed to create topic entity for writer '{}' ({})",
                 topic, topicEntity);
         return nullptr;
      }
      struct ddsi_sertype *sertypeForWriter = sertype;

      dds_qos_t *qos = dds_create_qos();
      dds_reliability_kind_t reliabilityKind = DDS_RELIABILITY_RELIABLE;
      if (reliability == "best_effort")
         reliabilityKind = DDS_RELIABILITY_BEST_EFFORT;

      dds_durability_kind_t durabilityKind = DDS_DURABILITY_VOLATILE;
      if (durability == "transient_local")
         durabilityKind = DDS_DURABILITY_TRANSIENT_LOCAL;
      else if (durability == "transient")
         durabilityKind = DDS_DURABILITY_TRANSIENT;
      else if (durability == "persistent")
         durabilityKind = DDS_DURABILITY_PERSISTENT;

      dds_qset_reliability(qos, reliabilityKind, DDS_SECS(1));
      dds_qset_durability(qos, durabilityKind);
      if (historyDepth < 0)
      {
         dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, 0);
      }
      else
      {
         dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, std::max<int32_t>(1, historyDepth));
      }

      dds_entity_t writer = dds_create_writer(publisher, topicEntity, qos, nullptr);
      dds_delete_qos(qos);
      dds_delete(topicEntity); // writer holds its own reference

      if (writer < 0)
      {
         GPERROR("OmniscopeDds: failed to create writer for '{}' ({})", topic, writer);
         return nullptr;
      }

      std::lock_guard lock(mutex);
      auto [it, inserted] = activeWriters.emplace(
         topic, ActiveWriter{.writer = writer, .sertype = sertypeForWriter});
      GPINFO("OmniscopeDds: created replay writer for '{}'", topic);
      return &it->second;
   }
};

// ---------------------------------------------------------------------------
//  Raw CDR polling helper
// ---------------------------------------------------------------------------

static void pollRawCdr(dds_entity_t reader, const std::string &topic,
                       const std::string &typeName,
                       const std::string &reliability,
                       const std::string &durability,
                       int32_t historyDepth,
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
            R"({{"raw_cdr":"{}","byte_count":{},"type_name":"{}","qos":{{"reliability":"{}","durability":"{}","history_depth":{}}}}})",
            hex, sz, typeName, reliability, durability, historyDepth);

         callback(topic, json);
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
   }
}

// ---------------------------------------------------------------------------
//  Replay helpers
// ---------------------------------------------------------------------------

// Inverse of pollRawCdr's hex encoder. Returns false on malformed input
// (odd length or non-hex characters) without touching `out`.
static bool hexDecode(const std::string &hex, std::vector<uint8_t> &out)
{
   if (hex.size() % 2 != 0) return false;

   auto nibble = [](char c) -> int
   {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
   };

   std::vector<uint8_t> decoded;
   decoded.reserve(hex.size() / 2);
   for (size_t i = 0; i < hex.size(); i += 2)
   {
      int hi = nibble(hex[i]);
      int lo = nibble(hex[i + 1]);
      if (hi < 0 || lo < 0) return false;
      decoded.push_back(static_cast<uint8_t>((hi << 4) | lo));
   }

   out = std::move(decoded);
   return true;
}

// ---------------------------------------------------------------------------
//  TransportDds
// ---------------------------------------------------------------------------

TransportDds::TransportDds(uint32_t domainId)
   : _impl(std::make_unique<Impl>())
{
   _impl->participant = dds_create_participant(
      static_cast<dds_domainid_t>(domainId), nullptr, nullptr);

   if (_impl->participant < 0)
   {
      GPERROR("OmniscopeDds: failed to create participant on domain {} ({})",
              domainId, _impl->participant);
      return;
   }

   _impl->subscriber = dds_create_subscriber(_impl->participant, nullptr, nullptr);
   _impl->publisher  = dds_create_publisher(_impl->participant, nullptr, nullptr);
   if (_impl->publisher < 0)
   {
      GPERROR("OmniscopeDds: failed to create publisher on domain {} ({}) — "
              "playback will not work", domainId, _impl->publisher);
   }

   _impl->builtinReader = std::make_unique<CycloneDDS::BuiltinTopicReader>(
      _impl->participant,
      [this](const CycloneDDS::DiscoveredTopic &dt, bool appeared,
             dds_instance_handle_t handle)
      { _impl->onDiscovery(dt, appeared, handle); });

   GPINFO("OmniscopeDds: listening on domain {}", domainId);
}

TransportDds::~TransportDds()
{
   // Stop subscriptions and writers before tearing down DDS entities.
   {
      std::lock_guard lock(_impl->mutex);
      _impl->activeSubs.clear();
      for (auto &[topic, aw] : _impl->activeWriters)
      {
         if (aw.writer > 0) dds_delete(aw.writer);
      }
      _impl->activeWriters.clear();
   }
   _impl->builtinReader.reset();

   if (_impl->publisher > 0) dds_delete(_impl->publisher);
   if (_impl->subscriber > 0) dds_delete(_impl->subscriber);
   if (_impl->participant > 0) dds_delete(_impl->participant);
}

std::string TransportDds::name() const { return "DDS"; }

std::vector<std::string> TransportDds::topicNames() const
{
   std::lock_guard lock(_impl->mutex);
   std::vector<std::string> names;
   names.reserve(_impl->discoveredTopics.size());
   for (const auto &[name, _] : _impl->discoveredTopics)
      names.push_back(name);
   return names;
}

void TransportDds::setTopicsChangedCallback(TopicsChangedCallback callback)
{
   _impl->topicsChangedCb = std::move(callback);
}

void TransportDds::subscribe(const std::string &topic, MessageCallback callback)
{
   {
      std::lock_guard lock(_impl->mutex);
      if (_impl->activeSubs.count(topic))
         return; // already subscribed
   }

   // Retrieve the type name for the JSON envelope.
   std::string typeName;
   std::string reliability = "reliable";
   std::string durability = "volatile";
   int32_t historyDepth = 1;
   {
      std::lock_guard lock(_impl->mutex);
      auto it = _impl->discoveredTopics.find(topic);
      if (it != _impl->discoveredTopics.end())
      {
         typeName = it->second.typeName;
         reliability = it->second.reliability;
         durability = it->second.durability;
         historyDepth = it->second.historyDepth;
      }
   }

   // Create a blob sertype that accepts any CDR bytes without type-checking.
   // This bypasses dds_find_topic entirely — we don't need the publisher to
   // respond to type lookup requests; we just want the raw wire bytes.
   const char *sertypeName = typeName.empty() ? topic.c_str() : typeName.c_str();
   struct ddsi_sertype *sertype = blobSertypeCreate(sertypeName);
   if (!sertype)
   {
      GPERROR("OmniscopeDds: out of memory creating blob sertype for '{}'", topic);
      return;
   }

   // dds_create_topic_sertype transfers ownership of sertype to the domain on
   // success (and sets sertype = nullptr). On failure, caller still owns it.
   dds_entity_t topicEntity = dds_create_topic_sertype(
      _impl->participant, topic.c_str(), &sertype, nullptr, nullptr, nullptr);

   if (topicEntity < 0)
   {
      if (sertype) ddsi_sertype_unref(sertype);
      GPERROR("OmniscopeDds: failed to create topic entity for '{}' ({})", topic, topicEntity);
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
      GPERROR("OmniscopeDds: failed to create reader for '{}' ({})", topic, reader);
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

   std::lock_guard lock(_impl->mutex);
   _impl->activeSubs[topic] = std::move(sub);

   GPINFO("OmniscopeDds: subscribed to '{}'", topic);
}

void TransportDds::unsubscribe(const std::string &topic)
{
   std::lock_guard lock(_impl->mutex);
   _impl->activeSubs.erase(topic);
   GPINFO("OmniscopeDds: unsubscribed from '{}'", topic);
}

bool TransportDds::isSubscribed(const std::string &topic) const
{
   std::lock_guard lock(_impl->mutex);
   return _impl->activeSubs.count(topic) > 0;
}

void TransportDds::publishFromJson(const std::string &topic,
                                    const std::string &jsonData)
{
   auto j = crow::json::load(jsonData);
   if (!j)
   {
      GPERROR("OmniscopeDds: publishFromJson('{}') - invalid JSON", topic);
      return;
   }

   std::vector<uint8_t> bytes;
   if (!j.has("raw_cdr") || !hexDecode(j["raw_cdr"].s(), bytes))
   {
      GPERROR("OmniscopeDds: publishFromJson('{}') - missing or malformed raw_cdr", topic);
      return;
   }

   std::string typeName = j.has("type_name") ? std::string(j["type_name"].s()) : std::string();

   std::string reliability = "reliable";
   std::string durability = "volatile";
   int32_t historyDepth = 1;
   if (j.has("qos"))
   {
      auto qos = j["qos"];
      if (qos.has("reliability")) reliability = std::string(qos["reliability"].s());
      if (qos.has("durability")) durability = std::string(qos["durability"].s());
      if (qos.has("history_depth")) historyDepth = static_cast<int32_t>(qos["history_depth"].i());
   }

   ActiveWriter *aw = _impl->getOrCreateWriter(topic, typeName, reliability, durability,
                                               historyDepth);
   if (!aw) return;

   ddsrt_iovec_t iov;
   iov.iov_base = bytes.data();
   iov.iov_len  = bytes.size();

   struct ddsi_serdata *serdata = ddsi_serdata_from_ser_iov(
      aw->sertype, SDK_DATA, 1, &iov, bytes.size());
   if (!serdata)
   {
      GPERROR("OmniscopeDds: publishFromJson('{}') - failed to build serdata", topic);
      return;
   }

   dds_return_t rc = dds_writecdr(aw->writer, serdata);
   if (rc != DDS_RETCODE_OK)
   {
      GPERROR("OmniscopeDds: publishFromJson('{}') - dds_writecdr failed ({})", topic, rc);
      return;
   }

   GPINFO("OmniscopeDds: replayed {} bytes on '{}'", bytes.size(), topic);
}

} // namespace Omniscope
