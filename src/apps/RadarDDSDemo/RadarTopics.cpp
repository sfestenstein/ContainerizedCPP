#include "RadarTopics.h"

#include <dds/dds.hpp>

namespace RadarDemo
{

namespace
{

CycloneDDS::TopicEntry makeReliableVolatileKeepLast(const std::string &topicName, uint32_t depth)
{
   dds::pub::qos::DataWriterQos writerQos;
   writerQos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(1))
             << dds::core::policy::Durability::Volatile()
             << dds::core::policy::History::KeepLast(depth);

   dds::sub::qos::DataReaderQos readerQos;
   readerQos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(1))
             << dds::core::policy::Durability::Volatile()
             << dds::core::policy::History::KeepLast(depth);

   return {.topicName = topicName, .writerQos = writerQos, .readerQos = readerQos};
}

CycloneDDS::TopicEntry makeBestEffortVolatileKeepLast1(const std::string &topicName)
{
   dds::pub::qos::DataWriterQos writerQos;
   writerQos << dds::core::policy::Reliability::BestEffort()
             << dds::core::policy::Durability::Volatile()
             << dds::core::policy::History::KeepLast(1);

   dds::sub::qos::DataReaderQos readerQos;
   readerQos << dds::core::policy::Reliability::BestEffort()
             << dds::core::policy::Durability::Volatile()
             << dds::core::policy::History::KeepLast(1);

   return {.topicName = topicName, .writerQos = writerQos, .readerQos = readerQos};
}

CycloneDDS::TopicEntry makeReliableTransientLocalKeepLast1(const std::string &topicName)
{
   dds::pub::qos::DataWriterQos writerQos;
   writerQos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(1))
             << dds::core::policy::Durability::TransientLocal()
             << dds::core::policy::History::KeepLast(1);

   dds::sub::qos::DataReaderQos readerQos;
   readerQos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(1))
             << dds::core::policy::Durability::TransientLocal()
             << dds::core::policy::History::KeepLast(1);

   return {.topicName = topicName, .writerQos = writerQos, .readerQos = readerQos};
}

CycloneDDS::TopicEntry makeReliableTransientLocalKeepAll(const std::string &topicName,
                                                          int32_t maxSamplesPerInstance)
{
   dds::pub::qos::DataWriterQos writerQos;
   writerQos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(1))
             << dds::core::policy::Durability::TransientLocal()
             << dds::core::policy::History::KeepAll()
             << dds::core::policy::ResourceLimits(-1, -1, maxSamplesPerInstance);

   dds::sub::qos::DataReaderQos readerQos;
   readerQos << dds::core::policy::Reliability::Reliable(dds::core::Duration::from_secs(1))
             << dds::core::policy::Durability::TransientLocal()
             << dds::core::policy::History::KeepAll()
             << dds::core::policy::ResourceLimits(-1, -1, maxSamplesPerInstance);

   return {.topicName = topicName, .writerQos = writerQos, .readerQos = readerQos};
}

} // namespace

RadarTopics::RadarTopics()
   : _config({
        makeReliableVolatileKeepLast(std::string(COMMAND_TOPIC), 10),
        makeReliableVolatileKeepLast(std::string(COMMAND_STATUS_TOPIC), 10),
        makeBestEffortVolatileKeepLast1(std::string(RADAR_TRACK_TOPIC)),
        makeReliableTransientLocalKeepLast1(std::string(COMPONENT_STATUS_TOPIC)),
        makeReliableTransientLocalKeepAll(std::string(RADAR_ALERT_TOPIC), 50),
     })
{
}

const CycloneDDS::DDSTopicConfig &RadarTopics::config() const
{
   return _config;
}

} // namespace RadarDemo
