#include "CycloneDDS/CycloneRawTopicPublisher.h"

#include "CycloneDDS/BlobSertype.h"
#include "CommonUtils/GeneralLogger.h"

#include <dds/dds.h>
#include <dds/ddsi/ddsi_serdata.h>
#include <dds/ddsi/ddsi_sertype.h>

#include <algorithm>
#include <cstdint>

namespace CycloneDDS
{

CycloneRawTopicPublisher::CycloneRawTopicPublisher(dds_entity_t participant, dds_entity_t publisher)
   : _participant(participant)
   , _publisher(publisher)
{
}

CycloneRawTopicPublisher::~CycloneRawTopicPublisher()
{
   std::lock_guard lock(_mutex);
   for (auto &[topic, aw] : _activeWriters)
   {
      if (aw.writer > 0) dds_delete(aw.writer);
   }
   _activeWriters.clear();
}

CycloneRawTopicPublisher::ActiveWriter *
CycloneRawTopicPublisher::getOrCreateWriter(const std::string &topic, const DdsCore::RawSample &sample)
{
   {
      std::lock_guard lock(_mutex);
      auto it = _activeWriters.find(topic);
      if (it != _activeWriters.end()) return &it->second;
   }

   const std::string &sertypeName = sample.typeName.empty() ? topic : sample.typeName;
   struct ddsi_sertype *sertype = blobSertypeCreate(sertypeName.c_str());
   if (!sertype)
   {
      GPERROR("CycloneRawTopicPublisher: out of memory creating blob sertype for writer '{}'", topic);
      return nullptr;
   }

   // dds_create_topic_sertype takes ownership of *sertype on success and
   // writes back, in the same out-param, a pointer to whichever sertype is
   // actually bound to the topic (see the class-level comment in the
   // header for why this may be a pre-existing sertype from a
   // CycloneRawTopicSubscriber sharing our participant). Only read the
   // pointer back *after* the call succeeds; never keep the pre-call value.
   dds_entity_t topicEntity = dds_create_topic_sertype(
      _participant, topic.c_str(), &sertype, nullptr, nullptr, nullptr);

   if (topicEntity < 0)
   {
      if (sertype) ddsi_sertype_unref(sertype);
      GPERROR("CycloneRawTopicPublisher: failed to create topic entity for writer '{}' ({})",
              topic, topicEntity);
      return nullptr;
   }
   struct ddsi_sertype *sertypeForWriter = sertype;

   dds_qos_t *qos = dds_create_qos();
   dds_reliability_kind_t reliabilityKind = DDS_RELIABILITY_RELIABLE;
   if (sample.reliability == "best_effort")
      reliabilityKind = DDS_RELIABILITY_BEST_EFFORT;

   dds_durability_kind_t durabilityKind = DDS_DURABILITY_VOLATILE;
   if (sample.durability == "transient_local")
      durabilityKind = DDS_DURABILITY_TRANSIENT_LOCAL;
   else if (sample.durability == "transient")
      durabilityKind = DDS_DURABILITY_TRANSIENT;
   else if (sample.durability == "persistent")
      durabilityKind = DDS_DURABILITY_PERSISTENT;

   dds_qset_reliability(qos, reliabilityKind, DDS_SECS(1));
   dds_qset_durability(qos, durabilityKind);
   if (sample.historyDepth < 0)
   {
      dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, 0);
   }
   else
   {
      dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, std::max<int32_t>(1, sample.historyDepth));
   }

   dds_entity_t writer = dds_create_writer(_publisher, topicEntity, qos, nullptr);
   dds_delete_qos(qos);
   dds_delete(topicEntity); // writer holds its own reference

   if (writer < 0)
   {
      GPERROR("CycloneRawTopicPublisher: failed to create writer for '{}' ({})", topic, writer);
      return nullptr;
   }

   std::lock_guard lock(_mutex);
   auto [it, inserted] = _activeWriters.emplace(
      topic, ActiveWriter{.writer = writer, .sertype = sertypeForWriter});
   GPINFO("CycloneRawTopicPublisher: created replay writer for '{}'", topic);
   return &it->second;
}

bool CycloneRawTopicPublisher::publish(const std::string &topic, const DdsCore::RawSample &sample)
{
   ActiveWriter *aw = getOrCreateWriter(topic, sample);
   if (!aw) return false;

   ddsrt_iovec_t iov;
   // ddsi_serdata_from_ser_iov only reads from iov to build the serdata; the
   // DDS C API is not const-correct, hence the cast.
   iov.iov_base = const_cast<uint8_t *>(sample.bytes.data());
   iov.iov_len  = sample.bytes.size();

   struct ddsi_serdata *serdata = ddsi_serdata_from_ser_iov(
      aw->sertype, SDK_DATA, 1, &iov, sample.bytes.size());
   if (!serdata)
   {
      GPERROR("CycloneRawTopicPublisher: publish('{}') - failed to build serdata", topic);
      return false;
   }

   dds_return_t rc = dds_writecdr(aw->writer, serdata);
   if (rc != DDS_RETCODE_OK)
   {
      GPERROR("CycloneRawTopicPublisher: publish('{}') - dds_writecdr failed ({})", topic, rc);
      return false;
   }

   GPINFO("CycloneRawTopicPublisher: published {} bytes on '{}'", sample.bytes.size(), topic);
   return true;
}

} // namespace CycloneDDS
