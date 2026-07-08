#ifndef FASTDDSPUBLISHER_H_
#define FASTDDSPUBLISHER_H_

// Project headers
#include "CommonUtils/GeneralLogger.h"
#include "FastDDS/FastDDSTopicConfig.h"

// Fast DDS headers
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>

// System headers
#include <stdexcept>
#include <string>

namespace FastDDS
{

/**
 * @brief Generic single-topic DDS publisher using eProsima Fast DDS.
 *
 * Constructed with a TopicEntry that defines the topic name and writer
 * QoS.  The Topic and DataWriter are lazily created on the first
 * publish() call.
 *
 * @tparam T         The fastddsgen-generated data type to publish.
 * @tparam TypeSupportT The fastddsgen-generated `...PubSubType` for T.
 *                   Fast DDS keeps the data type and its type support as
 *                   two separate generated classes, so (unlike Cyclone
 *                   DDS) both must be named explicitly here.
 *
 * Usage:
 * @code
 *    FastDDS::TopicEntry entry{"SensorTopic", writerQos, readerQos};
 *    FastDDS::FastDDSPublisher<SensorReading, SensorReadingPubSubType> pub(0, entry);
 *    pub.publish(msg);
 * @endcode
 */
template <typename T, typename TypeSupportT>
class FastDDSPublisher
{
public:
   /**
    * @brief Construct a Fast DDS publisher for a single topic.
    *
    * @param domainId DDS domain ID (participants on the same domain discover each other)
    * @param entry    TopicEntry defining the topic name and writer QoS
    * @param participantName Human-readable participant name (visible to DDS discovery)
    */
   FastDDSPublisher(uint32_t domainId,
                     TopicEntry entry,
                     const std::string &participantName = "")
      : _entry(std::move(entry))
      , _type(new TypeSupportT())
   {
      eprosima::fastdds::dds::DomainParticipantQos pqos =
         eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT;
      if (!participantName.empty())
      {
         pqos.name(participantName);
      }

      _participant = eprosima::fastdds::dds::DomainParticipantFactory::get_instance()
                        ->create_participant(domainId, pqos);
      if (!_participant)
      {
         throw std::runtime_error("FastDDSPublisher: failed to create DomainParticipant");
      }

      _type.register_type(_participant);

      _publisher = _participant->create_publisher(eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT);
      if (!_publisher)
      {
         throw std::runtime_error("FastDDSPublisher: failed to create Publisher");
      }

      GPINFO("FastDDSPublisher created: domain={}, topic={}, name={}",
             domainId, _entry.topicName, participantName);
   }

   ~FastDDSPublisher()
   {
      if (_publisher)
      {
         if (_writer)
         {
            _publisher->delete_datawriter(_writer);
         }
         _participant->delete_publisher(_publisher);
      }
      if (_topic)
      {
         _participant->delete_topic(_topic);
      }
      if (_participant)
      {
         eprosima::fastdds::dds::DomainParticipantFactory::get_instance()
            ->delete_participant(_participant);
      }
   }

   // Non-copyable, non-movable (owns factory-allocated pointers)
   FastDDSPublisher(const FastDDSPublisher &) = delete;
   FastDDSPublisher &operator=(const FastDDSPublisher &) = delete;
   FastDDSPublisher(FastDDSPublisher &&) = delete;
   FastDDSPublisher &operator=(FastDDSPublisher &&) = delete;

   /**
    * @brief Publish a message on the configured topic.
    *
    * The Topic and DataWriter are lazily created on the first call.
    *
    * @param message The data sample to publish
    */
   void publish(const T &message)
   {
      auto *writer = getOrCreateWriter();
      // Fast DDS's DataWriter::write() takes a non-const void* — the sample
      // is serialized immediately and not retained or mutated.
      writer->write(const_cast<T *>(&message));
   }

   /**
    * @brief Get the underlying DDS participant.
    */
   [[nodiscard]] eprosima::fastdds::dds::DomainParticipant &participant()
   {
      return *_participant;
   }

   /**
    * @brief Get the topic entry.
    */
   [[nodiscard]] const TopicEntry &topicEntry() const
   {
      return _entry;
   }

private:
   eprosima::fastdds::dds::DataWriter *getOrCreateWriter()
   {
      if (!_writer)
      {
         _topic = _participant->create_topic(
            _entry.topicName, _type.get_type_name(), eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
         if (!_topic)
         {
            throw std::runtime_error("FastDDSPublisher: failed to create Topic");
         }

         _writer = _publisher->create_datawriter(_topic, _entry.writerQos);
         if (!_writer)
         {
            throw std::runtime_error("FastDDSPublisher: failed to create DataWriter");
         }
         GPINFO("FastDDSPublisher: created writer for topic '{}'", _entry.topicName);
      }
      return _writer;
   }

   TopicEntry _entry;
   eprosima::fastdds::dds::TypeSupport _type;
   eprosima::fastdds::dds::DomainParticipant *_participant = nullptr;
   eprosima::fastdds::dds::Publisher *_publisher = nullptr;
   eprosima::fastdds::dds::Topic *_topic = nullptr;
   eprosima::fastdds::dds::DataWriter *_writer = nullptr;
};

} // namespace FastDDS

#endif // FASTDDSPUBLISHER_H_
