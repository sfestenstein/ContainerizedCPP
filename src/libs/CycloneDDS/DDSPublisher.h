#ifndef DDSPUBLISHER_H_
#define DDSPUBLISHER_H_

// Project headers
#include "CommonUtils/GeneralLogger.h"
#include "CycloneDDS/DDSTopicConfig.h"

// Cyclone DDS C++ headers
#include <dds/dds.hpp>

// System headers
#include <optional>
#include <string>

namespace CycloneDDS
{

/**
 * @brief Generic single-topic DDS publisher using Cyclone DDS.
 *
 * Constructed with a TopicEntry that defines the topic name and writer
 * QoS.  The DataWriter is lazily created on the first publish() call.
 *
 * @tparam T The IDL-generated DDS data type to publish.
 *
 * Usage:
 * @code
 *    CycloneDDS::TopicEntry entry{"SensorTopic", writerQos, readerQos};
 *    CycloneDDS::DDSPublisher<dds_messages::SensorReading> pub(0, entry);
 *    pub.publish(msg);
 * @endcode
 */
template <typename T>
class DDSPublisher
{
public:
   /**
    * @brief Construct a DDS publisher for a single topic.
    *
    * @param domainId DDS domain ID (participants on the same domain discover each other)
    * @param entry    TopicEntry defining the topic name and writer QoS
    * @param participantName Human-readable name (logged, not used by DDS)
    */
   DDSPublisher(uint32_t domainId,
                TopicEntry entry,
                const std::string &participantName = "")
      : _participant(domainId)
      , _publisher(_participant)
      , _entry(std::move(entry))
   {
      GPINFO("DDSPublisher created: domain={}, topic={}, name={}",
             domainId, _entry.topicName, participantName);
   }

   ~DDSPublisher() = default;

   // Non-copyable, non-movable
   DDSPublisher(const DDSPublisher &) = delete;
   DDSPublisher &operator=(const DDSPublisher &) = delete;
   DDSPublisher(DDSPublisher &&) = delete;
   DDSPublisher &operator=(DDSPublisher &&) = delete;

   /**
    * @brief Publish a message on the configured topic.
    *
    * The DataWriter is lazily created on the first call.
    *
    * @param message The data sample to publish
    */
   void publish(const T &message)
   {
      auto &writer = getOrCreateWriter();
      writer.write(message);
   }

   /**
    * @brief Get the underlying DDS participant.
    */
   [[nodiscard]] dds::domain::DomainParticipant &participant()
   {
      return _participant;
   }

   /**
    * @brief Get the topic entry.
    */
   [[nodiscard]] const TopicEntry &topicEntry() const
   {
      return _entry;
   }

private:
   using WriterType = dds::pub::DataWriter<T>;
   using TopicType = dds::topic::Topic<T>;

   WriterType &getOrCreateWriter()
   {
      if (!_writer)
      {
         auto topic = TopicType(_participant, _entry.topicName);
         _writer.emplace(_publisher, topic, _entry.writerQos);
         GPINFO("DDSPublisher: created writer for topic '{}'", _entry.topicName);
      }
      return *_writer;
   }

   dds::domain::DomainParticipant _participant;
   dds::pub::Publisher _publisher;
   TopicEntry _entry;
   std::optional<WriterType> _writer;
};

} // namespace CycloneDDS

#endif // DDSPUBLISHER_H_
