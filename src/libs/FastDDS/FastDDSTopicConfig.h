#ifndef FASTDDSTOPICCONFIG_H_
#define FASTDDSTOPICCONFIG_H_

// Fast DDS headers
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>

// System headers
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace FastDDS
{

/**
 * @brief Describes a single DDS topic: its name and the QoS policies for
 *        both the DataWriter and DataReader side.
 *
 * The writer and reader QoS are stored together so that a single
 * TopicEntry is the authoritative source of truth for how a topic is
 * configured — guaranteeing RxO (Request-vs-Offered) compatibility.
 */
struct TopicEntry
{
   std::string topicName;
   eprosima::fastdds::dds::DataWriterQos writerQos;
   eprosima::fastdds::dds::DataReaderQos readerQos;
};

/**
 * @brief Central registry of DDS topics and their QoS policies.
 *
 * A FastDDSTopicConfig is constructed with a list of TopicEntry objects
 * that define every topic a publisher or subscriber is allowed to use.
 * Both FastDDSPublisher and FastDDSSubscriber accept a TopicEntry so the
 * QoS is guaranteed to match.
 *
 * Usage:
 * @code
 *    FastDDS::FastDDSTopicConfig config({
 *       {"SensorTopic", wQos, rQos},
 *    });
 *
 *    auto entry = config.getEntry("SensorTopic");
 *    FastDDS::FastDDSPublisher<SensorReading, SensorReadingPubSubType> pub(0, entry);
 * @endcode
 */
class FastDDSTopicConfig
{
public:
   /**
    * @brief Construct from a list of topic entries.
    */
   FastDDSTopicConfig(std::initializer_list<TopicEntry> entries);

   /**
    * @brief Construct from a vector of topic entries.
    */
   explicit FastDDSTopicConfig(const std::vector<TopicEntry> &entries);

   /**
    * @brief Look up the full TopicEntry for a given topic name.
    * @throws std::out_of_range if the topic is not registered.
    */
   [[nodiscard]] const TopicEntry &getEntry(const std::string &topicName) const;

   /**
    * @brief Get the DataWriter QoS for a topic.
    * @throws std::out_of_range if the topic is not registered.
    */
   [[nodiscard]] const eprosima::fastdds::dds::DataWriterQos &
   writerQos(const std::string &topicName) const;

   /**
    * @brief Get the DataReader QoS for a topic.
    * @throws std::out_of_range if the topic is not registered.
    */
   [[nodiscard]] const eprosima::fastdds::dds::DataReaderQos &
   readerQos(const std::string &topicName) const;

   /**
    * @brief Check whether a topic is registered.
    */
   [[nodiscard]] bool hasTopic(const std::string &topicName) const;

   /**
    * @brief Return all registered topic names.
    */
   [[nodiscard]] std::vector<std::string> topicNames() const;

private:
   std::unordered_map<std::string, TopicEntry> _entries;
};

} // namespace FastDDS

#endif // FASTDDSTOPICCONFIG_H_
