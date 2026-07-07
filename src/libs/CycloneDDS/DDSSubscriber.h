#ifndef DDSSUBSCRIBER_H_
#define DDSSUBSCRIBER_H_

// Project headers
#include "CommonUtils/GeneralLogger.h"
#include "CycloneDDS/DDSTopicConfig.h"

// Cyclone DDS C++ headers
#include <dds/dds.hpp>

// System headers
#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <thread>

namespace CycloneDDS
{

/**
 * @brief Generic single-topic DDS subscriber using Cyclone DDS.
 *
 * Constructed with a TopicEntry that defines the topic name and reader
 * QoS.  Call subscribe() with a handler, then start() to begin polling.
 *
 * @tparam T The IDL-generated DDS data type to subscribe to.
 *
 * Usage:
 * @code
 *    CycloneDDS::TopicEntry entry{"SensorTopic", writerQos, readerQos};
 *    CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(0, entry);
 *    sub.subscribe([](const dds_messages::SensorReading &msg) {
 *       std::cout << "Received: " << msg.sensor_id() << std::endl;
 *    });
 *    sub.start();
 *    // ... run until done ...
 *    sub.stop();
 * @endcode
 */
template <typename T>
class DDSSubscriber
{
public:
   /**
    * @brief Callback type for received messages.
    */
   using MessageHandler = std::function<void(const T &)>;

   /**
    * @brief Construct a DDS subscriber for a single topic.
    *
    * @param domainId DDS domain ID (must match the publisher's domain)
    * @param entry    TopicEntry defining the topic name and reader QoS
    * @param participantName Human-readable name (logged, not used by DDS)
    */
   DDSSubscriber(uint32_t domainId,
                 TopicEntry entry,
                 const std::string &participantName = "")
      : _participant(domainId)
      , _subscriber(_participant)
      , _entry(std::move(entry))
      , _running(false)
   {
      GPINFO("DDSSubscriber created: domain={}, topic={}, name={}",
             domainId, _entry.topicName, participantName);
   }

   ~DDSSubscriber()
   {
      stop();
   }

   // Non-copyable, non-movable (owns thread)
   DDSSubscriber(const DDSSubscriber &) = delete;
   DDSSubscriber &operator=(const DDSSubscriber &) = delete;
   DDSSubscriber(DDSSubscriber &&) = delete;
   DDSSubscriber &operator=(DDSSubscriber &&) = delete;

   /**
    * @brief Subscribe with a callback handler.
    *
    * The reader is created using the QoS from the TopicEntry.
    * Must be called before start().
    *
    * @param handler Callback invoked for each received sample
    */
   void subscribe(MessageHandler handler)
   {
      auto topic = dds::topic::Topic<T>(_participant, _entry.topicName);
      _reader.emplace(_subscriber, topic, _entry.readerQos);
      _handler = std::move(handler);

      GPINFO("DDSSubscriber: subscribed to topic '{}'", _entry.topicName);
   }

   /**
    * @brief Start the polling thread for receiving messages.
    */
   void start()
   {
      if (_running.exchange(true))
      {
         return; // Already running
      }

      _pollThread = std::thread([this]() { pollLoop(); });
      GPINFO("DDSSubscriber: polling started");
   }

   /**
    * @brief Stop the polling thread.
    */
   void stop()
   {
      if (!_running.exchange(false))
      {
         return; // Already stopped
      }

      if (_pollThread.joinable())
      {
         _pollThread.join();
      }
      GPINFO("DDSSubscriber: polling stopped");
   }

   /**
    * @brief Check if the subscriber is currently running.
    */
   [[nodiscard]] bool isRunning() const
   {
      return _running.load();
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
   void pollLoop()
   {
      while (_running.load())
      {
         if (_reader && _handler)
         {
            auto samples = _reader->take();
            for (const auto &sample : samples)
            {
               if (sample.info().valid())
               {
                  _handler(sample.data());
               }
            }
         }
         std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
   }

   dds::domain::DomainParticipant _participant;
   dds::sub::Subscriber _subscriber;
   TopicEntry _entry;
   std::optional<dds::sub::DataReader<T>> _reader;
   MessageHandler _handler;
   std::atomic<bool> _running;
   std::thread _pollThread;
};

} // namespace CycloneDDS

#endif // DDSSUBSCRIBER_H_
