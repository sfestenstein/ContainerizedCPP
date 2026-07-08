#include "RadarTopics.h"

#include <fastdds/dds/core/policy/QosPolicies.hpp>

namespace RadarDemo
{

using eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS;
using eprosima::fastdds::dds::KEEP_ALL_HISTORY_QOS;
using eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
using eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
using eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS;
using eprosima::fastdds::dds::VOLATILE_DURABILITY_QOS;

namespace
{

FastDDS::TopicEntry makeReliableVolatileKeepLast(const std::string &topicName, int32_t depth)
{
   eprosima::fastdds::dds::DataWriterQos writerQos;
   writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
   writerQos.durability().kind = VOLATILE_DURABILITY_QOS;
   writerQos.history().kind = KEEP_LAST_HISTORY_QOS;
   writerQos.history().depth = depth;

   eprosima::fastdds::dds::DataReaderQos readerQos;
   readerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
   readerQos.durability().kind = VOLATILE_DURABILITY_QOS;
   readerQos.history().kind = KEEP_LAST_HISTORY_QOS;
   readerQos.history().depth = depth;

   return {.topicName = topicName, .writerQos = writerQos, .readerQos = readerQos};
}

FastDDS::TopicEntry makeBestEffortVolatileKeepLast1(const std::string &topicName)
{
   eprosima::fastdds::dds::DataWriterQos writerQos;
   writerQos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
   writerQos.durability().kind = VOLATILE_DURABILITY_QOS;
   writerQos.history().kind = KEEP_LAST_HISTORY_QOS;
   writerQos.history().depth = 1;

   eprosima::fastdds::dds::DataReaderQos readerQos;
   readerQos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
   readerQos.durability().kind = VOLATILE_DURABILITY_QOS;
   readerQos.history().kind = KEEP_LAST_HISTORY_QOS;
   readerQos.history().depth = 1;

   return {.topicName = topicName, .writerQos = writerQos, .readerQos = readerQos};
}

FastDDS::TopicEntry makeReliableTransientLocalKeepLast1(const std::string &topicName)
{
   eprosima::fastdds::dds::DataWriterQos writerQos;
   writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
   writerQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
   writerQos.history().kind = KEEP_LAST_HISTORY_QOS;
   writerQos.history().depth = 1;

   eprosima::fastdds::dds::DataReaderQos readerQos;
   readerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
   readerQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
   readerQos.history().kind = KEEP_LAST_HISTORY_QOS;
   readerQos.history().depth = 1;

   return {.topicName = topicName, .writerQos = writerQos, .readerQos = readerQos};
}

FastDDS::TopicEntry makeReliableTransientLocalKeepAll(const std::string &topicName,
                                                       int32_t maxSamplesPerInstance)
{
   eprosima::fastdds::dds::DataWriterQos writerQos;
   writerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
   writerQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
   writerQos.history().kind = KEEP_ALL_HISTORY_QOS;
   writerQos.resource_limits().max_samples_per_instance = maxSamplesPerInstance;

   eprosima::fastdds::dds::DataReaderQos readerQos;
   readerQos.reliability().kind = RELIABLE_RELIABILITY_QOS;
   readerQos.durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
   readerQos.history().kind = KEEP_ALL_HISTORY_QOS;
   readerQos.resource_limits().max_samples_per_instance = maxSamplesPerInstance;

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

const FastDDS::FastDDSTopicConfig &RadarTopics::config() const
{
   return _config;
}

} // namespace RadarDemo
