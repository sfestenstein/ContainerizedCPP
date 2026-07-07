#include "CycloneDDS/CycloneDDSConfig.h"

// Cyclone DDS C++ headers (only needed here for QoS policy types)
#include <dds/dds.hpp>

namespace CycloneDDS
{

void CycloneDDSConfig::defaultInitialize()
{
   // --- SensorTopic: reliable delivery ------------------------------------
   dds::pub::qos::DataWriterQos sensorWriterQos;
   sensorWriterQos
      << dds::core::policy::Reliability::Reliable(
            dds::core::Duration::from_secs(1))
      << dds::core::policy::History::KeepLast(10);

   dds::sub::qos::DataReaderQos sensorReaderQos;
   sensorReaderQos
      << dds::core::policy::Reliability::Reliable(
            dds::core::Duration::from_secs(1))
      << dds::core::policy::History::KeepLast(10);

   // --- TrackTopic: best-effort, latest-value -----------------------------
   dds::pub::qos::DataWriterQos trackWriterQos;
   trackWriterQos
      << dds::core::policy::Reliability::BestEffort()
      << dds::core::policy::History::KeepLast(1);

   dds::sub::qos::DataReaderQos trackReaderQos;
   trackReaderQos
      << dds::core::policy::Reliability::BestEffort()
      << dds::core::policy::History::KeepLast(1);

   // --- Build the config --------------------------------------------------
   _config = DDSTopicConfig({
      {.topicName = std::string(SENSOR_TOPIC),
       .writerQos = sensorWriterQos,
       .readerQos = sensorReaderQos},
      {.topicName = std::string(TRACK_TOPIC),
       .writerQos = trackWriterQos,
       .readerQos = trackReaderQos},
   });
}

const DDSTopicConfig &CycloneDDSConfig::config() const
{
   return _config;
}

} // namespace CycloneDDS
