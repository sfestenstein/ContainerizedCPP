#include "TransportDds.h"

#include "CycloneDDS/CycloneDDSConfig.h"
#include "CycloneDDS/DDSPublisher.h"
#include "CycloneDDS/DDSSubscriber.h"
#include "CommonUtils/GeneralLogger.h"

#include "DdsJsonHelpers.h"

#include <crow/json.h>

#include <mutex>
#include <unordered_set>

namespace Omniscope
{

// ============================================================================
//  TransportDds::Impl
// ============================================================================

struct TransportDds::Impl
{
   explicit Impl(uint32_t domainIdArg)
      : domainId(domainIdArg)
   {
      ddsConfig.defaultInitialize();
      const auto &config = ddsConfig.config();

      sensorPub = std::make_unique<
         CycloneDDS::DDSPublisher<dds_messages::SensorReading>>(
         domainId, config.getEntry(std::string(CycloneDDS::SENSOR_TOPIC)),
         "MonitorSensorPub");
      trackPub = std::make_unique<
         CycloneDDS::DDSPublisher<dds_messages::TrackUpdate>>(
         domainId, config.getEntry(std::string(CycloneDDS::TRACK_TOPIC)),
         "MonitorTrackPub");

      sensorSub = std::make_unique<
         CycloneDDS::DDSSubscriber<dds_messages::SensorReading>>(
         domainId, config.getEntry(std::string(CycloneDDS::SENSOR_TOPIC)),
         "MonitorSensor");
      trackSub = std::make_unique<
         CycloneDDS::DDSSubscriber<dds_messages::TrackUpdate>>(
         domainId, config.getEntry(std::string(CycloneDDS::TRACK_TOPIC)),
         "MonitorTrack");
   }

   uint32_t domainId;
   CycloneDDS::CycloneDDSConfig ddsConfig;

   std::unique_ptr<CycloneDDS::DDSPublisher<dds_messages::SensorReading>> sensorPub;
   std::unique_ptr<CycloneDDS::DDSPublisher<dds_messages::TrackUpdate>> trackPub;
   std::unique_ptr<CycloneDDS::DDSSubscriber<dds_messages::SensorReading>> sensorSub;
   std::unique_ptr<CycloneDDS::DDSSubscriber<dds_messages::TrackUpdate>> trackSub;

   std::unordered_set<std::string> activeTopics;
   mutable std::mutex mutex;
};

// ============================================================================
//  TransportDds public API
// ============================================================================

TransportDds::TransportDds(uint32_t domainId)
   : _impl(std::make_unique<Impl>(domainId))
{
   GPINFO("TransportDds: created on domain {}", domainId);
}

TransportDds::~TransportDds()
{
   // Stop all active subscriptions
   for (const auto &topic : _impl->activeTopics)
   {
      if (topic == CycloneDDS::SENSOR_TOPIC) _impl->sensorSub->stop();
      else if (topic == CycloneDDS::TRACK_TOPIC) _impl->trackSub->stop();
   }
}

std::string TransportDds::name() const
{
   return "DDS";
}

std::vector<std::string> TransportDds::topicNames() const
{
   return {std::string(CycloneDDS::SENSOR_TOPIC),
           std::string(CycloneDDS::TRACK_TOPIC)};
}

void TransportDds::subscribe(const std::string &topic, MessageCallback callback)
{
   std::lock_guard lock(_impl->mutex);
   if (_impl->activeTopics.contains(topic)) return;

   if (topic == CycloneDDS::SENSOR_TOPIC && _impl->sensorSub)
   {
      _impl->sensorSub->subscribe(
         [callback](const dds_messages::SensorReading &msg) {
            callback(std::string(CycloneDDS::SENSOR_TOPIC),
               DdsJsonHelpers::toJson(msg).dump());
         });
      _impl->sensorSub->start();
      _impl->activeTopics.insert(topic);
   }
   else if (topic == CycloneDDS::TRACK_TOPIC && _impl->trackSub)
   {
      _impl->trackSub->subscribe(
         [callback](const dds_messages::TrackUpdate &msg) {
            callback(std::string(CycloneDDS::TRACK_TOPIC),
               DdsJsonHelpers::toJson(msg).dump());
         });
      _impl->trackSub->start();
      _impl->activeTopics.insert(topic);
   }

   GPINFO("TransportDds: subscribed to {}", topic);
}

void TransportDds::unsubscribe(const std::string &topic)
{
   std::lock_guard lock(_impl->mutex);
   if (!_impl->activeTopics.contains(topic)) return;

   if (topic == CycloneDDS::SENSOR_TOPIC && _impl->sensorSub)
   {
      _impl->sensorSub->stop();
      _impl->activeTopics.erase(topic);
   }
   else if (topic == CycloneDDS::TRACK_TOPIC && _impl->trackSub)
   {
      _impl->trackSub->stop();
      _impl->activeTopics.erase(topic);
   }

   GPINFO("TransportDds: unsubscribed from {}", topic);
}

bool TransportDds::isSubscribed(const std::string &topic) const
{
   std::lock_guard lock(_impl->mutex);
   return _impl->activeTopics.contains(topic);
}

void TransportDds::publishFromJson(const std::string &topic,
                                   const std::string &jsonData)
{
   auto data = crow::json::load(jsonData);
   if (!data) return;

   if (topic == CycloneDDS::SENSOR_TOPIC && _impl->sensorPub)
   {
      _impl->sensorPub->publish(
         DdsJsonHelpers::fromJson(data,
            static_cast<dds_messages::SensorReading const *>(nullptr)));
   }
   else if (topic == CycloneDDS::TRACK_TOPIC && _impl->trackPub)
   {
      _impl->trackPub->publish(
         DdsJsonHelpers::fromJson(data,
            static_cast<dds_messages::TrackUpdate const *>(nullptr)));
   }
}

} // namespace Omniscope
