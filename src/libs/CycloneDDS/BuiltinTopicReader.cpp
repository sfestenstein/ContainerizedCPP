#include "CycloneDDS/BuiltinTopicReader.h"

#include "CommonUtils/GeneralLogger.h"

#include <dds/dds.h>

#include <array>
#include <chrono>
#include <cstring>
#include <string_view>
#include <sstream>

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

static std::string guidToString(const dds_guid_t &guid)
{
   static constexpr char HEX[] = "0123456789abcdef";
   std::string out;
   out.reserve(36);
   for (size_t i = 0; i < 16; ++i)
   {
      const uint8_t b = guid.v[i];
      out.push_back(HEX[(b >> 4U) & 0x0FU]);
      out.push_back(HEX[b & 0x0FU]);
      if (i == 3 || i == 5 || i == 7 || i == 9)
         out.push_back('-');
   }
   return out;
}

static std::string endpointNameFromQos(const dds_qos_t *qos)
{
   char *entityName = nullptr;
   if (dds_qget_entity_name(qos, &entityName) && entityName != nullptr)
   {
      std::string name = entityName;
      dds_free(entityName);
      return name;
   }

   static constexpr const char *PROPERTY_KEYS[] = {
      "app_name",
      "application_name",
      "dds.application.name",
      "process_name",
      "program_name",
   };

   for (const char *key : PROPERTY_KEYS)
   {
      char *value = nullptr;
      if (dds_qget_prop(qos, key, &value) && value != nullptr)
      {
         std::string name = value;
         dds_free(value);
         return name;
      }
   }

   void *userDataRaw = nullptr;
   size_t userDataSize = 0;
   if (dds_qget_userdata(qos, &userDataRaw, &userDataSize) && userDataRaw != nullptr
       && userDataSize > 0)
   {
      std::string userData(static_cast<const char *>(userDataRaw), userDataSize);
      dds_free(userDataRaw);

      auto extractField = [&](std::string_view key) -> std::string
      {
         const std::string marker = std::string(key) + "=";
         const size_t begin = userData.find(marker);
         if (begin == std::string::npos)
            return {};

         const size_t valueBegin = begin + marker.size();
         size_t valueEnd = userData.find(';', valueBegin);
         if (valueEnd == std::string::npos)
            valueEnd = userData.size();

         return userData.substr(valueBegin, valueEnd - valueBegin);
      };

      if (auto appName = extractField("app_name"); !appName.empty())
         return appName;

      if (auto endpointName = extractField("endpoint_name"); !endpointName.empty())
         return endpointName;

      if (auto applicationName = extractField("application_name"); !applicationName.empty())
         return applicationName;
   }

   return {};
}

// ---------------------------------------------------------------------------
//  BuiltinTopicReader
// ---------------------------------------------------------------------------

BuiltinTopicReader::BuiltinTopicReader(dds_entity_t participant, dds_entity_t builtinTopic,
                                       TopicDiscoveryCallback callback)
   : _reader(dds_create_reader(participant, builtinTopic, nullptr, nullptr))
   , _role(builtinTopic == DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION
              ? TopicEndpointRole::Subscriber
              : TopicEndpointRole::Publisher)
   , _callback(std::move(callback))
{
   if (_reader < 0)
   {
      GPERROR("BuiltinTopicReader: failed to create built-in reader ({})", _reader);
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
         dt.endpointId = guidToString(ep->key);
         dt.participantId = guidToString(ep->participant_key);
         dt.endpointName = endpointNameFromQos(ep->qos);

         _callback(dt, appeared, _role, infos[index].instance_handle);
      }

      if (n > 0)
         dds_return_loan(_reader, samples.data(), n);

      std::this_thread::sleep_for(std::chrono::milliseconds(200));
   }
}

} // namespace CycloneDDS
