#ifndef ITRANSPORT_H_
#define ITRANSPORT_H_

#include <functional>
#include <string>
#include <vector>

namespace Omniscope
{

/// Callback invoked when a subscribed topic receives a message.
/// Parameters: topic name, JSON-serialized message data.
using MessageCallback = std::function<void(const std::string &, const std::string &)>;

/// Callback invoked when the set of available topics changes (topics appear or disappear).
using TopicsChangedCallback = std::function<void()>;

/**
 * @brief Abstract transport interface for publish-subscribe middleware.
 *
 * Each transport implementation (DDS, Zyre, ZMQ, …) provides concrete
 * subscribe/unsubscribe/publish behaviour while the monitor application
 * works exclusively through this interface.
 */
class ITransport
{
public:
   virtual ~ITransport() = default;

   /// Human-readable name of this transport (e.g. "DDS", "Zyre").
   [[nodiscard]] virtual std::string name() const = 0;

   /// List of topic names this transport can handle.
   [[nodiscard]] virtual std::vector<std::string> topicNames() const = 0;

   /// Begin receiving messages on the given topic.
   virtual void subscribe(const std::string &topic, MessageCallback callback) = 0;

   /// Stop receiving messages on the given topic.
   virtual void unsubscribe(const std::string &topic) = 0;

   /// Returns true if currently subscribed to the given topic.
   [[nodiscard]] virtual bool isSubscribed(const std::string &topic) const = 0;

   /// Publish a message from a JSON string (used during playback).
   virtual void publishFromJson(const std::string &topic,
                                const std::string &jsonData) = 0;

   /// Register a callback to be invoked whenever the topic list changes.
   /// Static-topic transports may ignore this (default no-op).
   virtual void setTopicsChangedCallback(TopicsChangedCallback /*callback*/) {}
};

} // namespace Omniscope

#endif // ITRANSPORT_H_
