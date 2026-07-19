#ifndef DDSSUBSCRIBER_H_
#define DDSSUBSCRIBER_H_

// Project headers
#include "CommonUtils/GeneralLogger.h"
#include "CycloneDDS/DDSTopicConfig.h"
#include "Observability/InterfaceMetrics.h"

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
 * QoS.  Call subscribe() with a handler, then start() to begin receiving.
 *
 * Delivery is event-driven rather than polled: the background thread
 * blocks on a WaitSet attached to the reader's StatusCondition
 * (data_available), so samples are picked up as soon as they arrive
 * instead of on a fixed sleep interval. A GuardCondition on the same
 * WaitSet lets stop() wake the thread immediately rather than waiting
 * out a timeout.
 *
 * @tparam T The IDL-generated DDS data type to subscribe to.
 *
 * Usage:
 * @code
 *    CycloneDDS::TopicEntry entry{"RadarTrack", writerQos, readerQos};
 *    CycloneDDS::DDSSubscriber<radar_demo::RadarTrack> sub(0, entry);
 *    sub.subscribe([](const radar_demo::RadarTrack &msg) {
 *       std::cout << "Received: " << msg.track_id() << std::endl;
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
      , _interfaceName(participantName)
   {
      _waitSet.attach_condition(_stopGuard);
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
    * The reader is created using the QoS from the TopicEntry, and its
    * StatusCondition is attached to the WaitSet so the background thread
    * wakes as soon as data is available. Must be called before start().
    *
    * @param handler Callback invoked for each received sample
    */
   void subscribe(MessageHandler handler)
   {
      auto topic = dds::topic::Topic<T>(_participant, _entry.topicName);
      _reader.emplace(_subscriber, topic, _entry.readerQos);
      _handler = std::move(handler);

      _statusCondition.emplace(*_reader);
      _statusCondition->enabled_statuses(dds::core::status::StatusMask::data_available());
      _waitSet.attach_condition(*_statusCondition);

      GPINFO("DDSSubscriber: subscribed to topic '{}'", _entry.topicName);
   }

   /**
    * @brief Start the background thread that waits for and delivers messages.
    */
   void start()
   {
      if (_running.exchange(true))
      {
         return; // Already running
      }

      _pollThread = std::thread([this]() { waitLoop(); });
      GPINFO("DDSSubscriber: polling started");
   }

   /**
    * @brief Stop the background thread.
    */
   void stop()
   {
      if (!_running.exchange(false))
      {
         return; // Already stopped
      }

      // Wake the blocked wait() immediately rather than letting it run out
      // a timeout.
      _stopGuard.trigger_value(true);
      if (_pollThread.joinable())
      {
         _pollThread.join();
      }
      _stopGuard.trigger_value(false);
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
   void waitLoop()
   {
      while (_running.load())
      {
         // Blocks until either the reader's StatusCondition signals
         // data_available or stop() triggers _stopGuard -- no fixed
         // polling interval, and no wasted wakeups when idle.
         _waitSet.wait();

         if (_reader && _handler)
         {
            auto samples = _reader->take();
            for (const auto &sample : samples)
            {
               if (sample.info().valid())
               {
                  // sizeof(T) is a stand-in for the real wire size -- wrong
                  // for variable-length IDL types (strings/sequences), but
                  // good enough to prove the metrics pipeline end-to-end.
                  Observability::metrics().recordReceived(_interfaceName, Observability::InterfaceType::DDS,
                                                            _entry.topicName, sizeof(T));
                  _handler(sample.data());
               }
            }
         }
      }
   }

   dds::domain::DomainParticipant _participant;
   dds::sub::Subscriber _subscriber;
   TopicEntry _entry;
   std::optional<dds::sub::DataReader<T>> _reader;
   MessageHandler _handler;
   std::atomic<bool> _running;
   std::thread _pollThread;

   dds::core::cond::WaitSet _waitSet;
   dds::core::cond::GuardCondition _stopGuard;
   std::optional<dds::core::cond::StatusCondition> _statusCondition;
   std::string _interfaceName;
};

} // namespace CycloneDDS

#endif // DDSSUBSCRIBER_H_
