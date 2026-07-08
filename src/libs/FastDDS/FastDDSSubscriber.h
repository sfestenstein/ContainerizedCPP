#ifndef FASTDDSSUBSCRIBER_H_
#define FASTDDSSUBSCRIBER_H_

// Project headers
#include "CommonUtils/GeneralLogger.h"
#include "FastDDS/FastDDSTopicConfig.h"

// Fast DDS headers
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>

// System headers
#include <functional>
#include <stdexcept>
#include <string>

namespace FastDDS
{

/**
 * @brief Generic single-topic DDS subscriber using eProsima Fast DDS.
 *
 * Constructed with a TopicEntry that defines the topic name and reader
 * QoS.  Call subscribe() with a handler to create the Topic/DataReader
 * and begin receiving — unlike Cyclone DDS's poll-thread subscriber,
 * delivery is driven by Fast DDS's own DataReaderListener callback, so
 * there is no start()/stop() or background thread to manage.
 *
 * @tparam T         The fastddsgen-generated data type to subscribe to.
 * @tparam TypeSupportT The fastddsgen-generated `...PubSubType` for T.
 *
 * Usage:
 * @code
 *    FastDDS::TopicEntry entry{"SensorTopic", writerQos, readerQos};
 *    FastDDS::FastDDSSubscriber<SensorReading, SensorReadingPubSubType> sub(0, entry);
 *    sub.subscribe([](const SensorReading &msg) {
 *       std::cout << "Received: " << msg.sensor_id() << std::endl;
 *    });
 *    // ... run until done ...
 * @endcode
 */
template <typename T, typename TypeSupportT>
class FastDDSSubscriber
{
public:
   /**
    * @brief Callback type for received messages.
    */
   using MessageHandler = std::function<void(const T &)>;

   /**
    * @brief Construct a Fast DDS subscriber for a single topic.
    *
    * @param domainId DDS domain ID (must match the publisher's domain)
    * @param entry    TopicEntry defining the topic name and reader QoS
    * @param participantName Human-readable participant name (visible to DDS discovery)
    */
   FastDDSSubscriber(uint32_t domainId,
                      TopicEntry entry,
                      const std::string &participantName = "")
      : _entry(std::move(entry))
      , _type(new TypeSupportT())
      , _listener(_handler)
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
         throw std::runtime_error("FastDDSSubscriber: failed to create DomainParticipant");
      }

      _type.register_type(_participant);

      _subscriber = _participant->create_subscriber(eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT);
      if (!_subscriber)
      {
         throw std::runtime_error("FastDDSSubscriber: failed to create Subscriber");
      }

      GPINFO("FastDDSSubscriber created: domain={}, topic={}, name={}",
             domainId, _entry.topicName, participantName);
   }

   ~FastDDSSubscriber()
   {
      if (_subscriber)
      {
         if (_reader)
         {
            _subscriber->delete_datareader(_reader);
         }
         _participant->delete_subscriber(_subscriber);
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
   FastDDSSubscriber(const FastDDSSubscriber &) = delete;
   FastDDSSubscriber &operator=(const FastDDSSubscriber &) = delete;
   FastDDSSubscriber(FastDDSSubscriber &&) = delete;
   FastDDSSubscriber &operator=(FastDDSSubscriber &&) = delete;

   /**
    * @brief Subscribe with a callback handler.
    *
    * Creates the Topic and DataReader using the QoS from the TopicEntry.
    * Delivery begins immediately via Fast DDS's listener callback — there
    * is no separate start() step.
    *
    * @param handler Callback invoked for each received sample
    */
   void subscribe(MessageHandler handler)
   {
      _handler = std::move(handler);

      _topic = _participant->create_topic(
         _entry.topicName, _type.get_type_name(), eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
      if (!_topic)
      {
         throw std::runtime_error("FastDDSSubscriber: failed to create Topic");
      }

      _reader = _subscriber->create_datareader(_topic, _entry.readerQos, &_listener);
      if (!_reader)
      {
         throw std::runtime_error("FastDDSSubscriber: failed to create DataReader");
      }

      GPINFO("FastDDSSubscriber: subscribed to topic '{}'", _entry.topicName);
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
   /**
    * @brief Forwards on_data_available() to the subscriber's handler.
    *
    * Holds a reference to the (mutable) handler rather than a copy, so
    * subscribe() can install the handler after the listener is
    * constructed without needing a two-phase listener setup.
    */
   class Listener : public eprosima::fastdds::dds::DataReaderListener
   {
   public:
      explicit Listener(MessageHandler &handler)
         : _handler(handler)
      {
      }

      void on_data_available(eprosima::fastdds::dds::DataReader *reader) override
      {
         T data;
         eprosima::fastdds::dds::SampleInfo info;
         // ReturnCode_t is injected into the global namespace by
         // <fastrtps/types/TypesBase.h> (via a global-scope `using`
         // declaration in Fast DDS's own headers), not into
         // eprosima::fastdds::dds — so it must stay unqualified here.
         while (reader->take_next_sample(&data, &info) == ReturnCode_t::RETCODE_OK)
         {
            if (info.valid_data && _handler)
            {
               _handler(data);
            }
         }
      }

   private:
      MessageHandler &_handler;
   };

   TopicEntry _entry;
   eprosima::fastdds::dds::TypeSupport _type;
   MessageHandler _handler;
   Listener _listener;
   eprosima::fastdds::dds::DomainParticipant *_participant = nullptr;
   eprosima::fastdds::dds::Subscriber *_subscriber = nullptr;
   eprosima::fastdds::dds::Topic *_topic = nullptr;
   eprosima::fastdds::dds::DataReader *_reader = nullptr;
};

} // namespace FastDDS

#endif // FASTDDSSUBSCRIBER_H_
