#include "CycloneDDS/BuiltinTopicReader.h"

#include "CommonUtils/GeneralLogger.h"

#include <dds/dds.h>

#include <chrono>
#include <cstring>

namespace CycloneDDS
{

// ---------------------------------------------------------------------------
//  QoS helpers
// ---------------------------------------------------------------------------

static std::string reliabilityStr(const dds_qos_t *qos)
{
   dds_reliability_kind_t kind = DDS_RELIABILITY_BEST_EFFORT;
   dds_duration_t         maxBlock{};
   dds_qget_reliability(qos, &kind, &maxBlock);
   return kind == DDS_RELIABILITY_RELIABLE ? "reliable" : "best_effort";
}

static std::string durabilityStr(const dds_qos_t *qos)
{
   dds_durability_kind_t kind = DDS_DURABILITY_VOLATILE;
   dds_qget_durability(qos, &kind);
   switch (kind)
   {
      case DDS_DURABILITY_TRANSIENT_LOCAL: return "transient_local";
      case DDS_DURABILITY_TRANSIENT:       return "transient";
      case DDS_DURABILITY_PERSISTENT:      return "persistent";
      default:                             return "volatile";
   }
}

static int32_t historyDepthOf(const dds_qos_t *qos)
{
   dds_history_kind_t kind  = DDS_HISTORY_KEEP_LAST;
   int32_t            depth = 1;
   dds_qget_history(qos, &kind, &depth);
   return kind == DDS_HISTORY_KEEP_ALL ? -1 : depth;
}

// ---------------------------------------------------------------------------
//  BuiltinTopicReader
// ---------------------------------------------------------------------------

BuiltinTopicReader::BuiltinTopicReader(dds_entity_t participant,
                                       TopicDiscoveryCallback callback)
   : _reader(dds_create_reader(participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
                               nullptr, nullptr))
   , _callback(std::move(callback))
{
   if (_reader < 0)
   {
      GPERROR("BuiltinTopicReader: failed to create DCPSPublication reader ({})", _reader);
      _running = false;
      return;
   }
   _thread = std::thread(&BuiltinTopicReader::pollLoop, this);
}

BuiltinTopicReader::~BuiltinTopicReader()
{
   _running = false;
   if (_thread.joinable()) _thread.join();
   if (_reader > 0) dds_delete(_reader);
}

void BuiltinTopicReader::pollLoop()
{
   static constexpr size_t MAX_SAMPLES = 16;
   std::array<void*, MAX_SAMPLES> samples{};
   std::array<dds_sample_info_t, MAX_SAMPLES> infos{};

   while (_running.load())
   {
      const dds_return_t n = dds_take(_reader, samples.data(), infos.data(), MAX_SAMPLES, MAX_SAMPLES);

      for (dds_return_t i = 0; i < n; ++i)
      {
         const auto index = static_cast<size_t>(i);
         const auto *ep = static_cast<const dds_builtintopic_endpoint_t *>(samples[index]);
         if (ep == nullptr || ep->topic_name == nullptr) continue;

         const bool appeared = (infos[index].instance_state == DDS_IST_ALIVE);

         DiscoveredTopic dt;
         dt.name        = ep->topic_name;
         dt.typeName    = ep->type_name ? ep->type_name : "";
         dt.reliability = reliabilityStr(ep->qos);
         dt.durability  = durabilityStr(ep->qos);
         dt.historyDepth = historyDepthOf(ep->qos);

         _callback(dt, appeared, infos[index].instance_handle);
      }

      if (n > 0)
         dds_return_loan(_reader, samples.data(), n);

      std::this_thread::sleep_for(std::chrono::milliseconds(200));
   }
}

} // namespace CycloneDDS
