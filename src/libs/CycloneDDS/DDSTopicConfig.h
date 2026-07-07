#ifndef DDSTOPICCONFIG_H_
#define DDSTOPICCONFIG_H_

// Cyclone DDS C++ headers
#include <dds/dds.hpp>

// System headers
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace CycloneDDS
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
   dds::pub::qos::DataWriterQos writerQos;
   dds::sub::qos::DataReaderQos readerQos;
};

/**
 * @brief Central registry of DDS topics and their QoS policies.
 *
 * A DDSTopicConfig is constructed with a list of TopicEntry objects that
 * define every topic a publisher or subscriber is allowed to use.
 * Both the DDSPublisher and DDSSubscriber accept a shared
 * `const DDSTopicConfig &` so the QoS is guaranteed to match.
 *
 * Usage:
 * @code
 *    // Build QoS
 *    dds::pub::qos::DataWriterQos wQos;
 *    dds::sub::qos::DataReaderQos rQos;
 *    wQos << dds::core::policy::Reliability::Reliable();
 *    rQos << dds::core::policy::Reliability::Reliable();
 *
 *    CycloneDDS::DDSTopicConfig config({
 *       {"SensorTopic",  wQos, rQos},
 *       {"TrackTopic",   wQos, rQos},
 *    });
 *
 *    auto sensorEntry = config.getEntry("SensorTopic");
 *    CycloneDDS::DDSPublisher<dds_messages::SensorReading>  pub(0, sensorEntry);
 *    CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(0, sensorEntry);
 *
 *    pub.publish(msg);                       // topic known from entry
 *    sub.subscribe(handler);                 // topic known from entry
 * @endcode
 */
class DDSTopicConfig
{
public:
   /**
    * @brief Construct from a list of topic entries.
    */
   DDSTopicConfig(std::initializer_list<TopicEntry> entries);

   /**
    * @brief Construct from a vector of topic entries.
    */
   explicit DDSTopicConfig(const std::vector<TopicEntry> &entries);

   /**
    * @brief Look up the full TopicEntry for a given topic name.
    * @throws std::out_of_range if the topic is not registered.
    */
   [[nodiscard]] const TopicEntry &getEntry(const std::string &topicName) const;

   /**
    * @brief Get the DataWriter QoS for a topic.
    * @throws std::out_of_range if the topic is not registered.
    */
   [[nodiscard]] const dds::pub::qos::DataWriterQos &
   writerQos(const std::string &topicName) const;

   /**
    * @brief Get the DataReader QoS for a topic.
    * @throws std::out_of_range if the topic is not registered.
    */
   [[nodiscard]] const dds::sub::qos::DataReaderQos &
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

} // namespace CycloneDDS

#endif // DDSTOPICCONFIG_H_
