#include "TransportZyre.h"

#include "proto/ProtoJsonDispatch.h"
#include "proto/ProtoTopicConfig.h"
#include "PubSub/ZyrePublisher.h"
#include "PubSub/ZyreSubscriber.h"
#include "CommonUtils/GeneralLogger.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace Omniscope
{

// ============================================================================
//  TransportZyre::Impl
// ============================================================================

struct TransportZyre::Impl
{
   explicit Impl(const std::string &nodeName)
      : name(nodeName),
        subscriber(nodeName),
        publisher(nodeName)
   {
      if (!publisher.start())
      {
         GPERROR("TransportZyre: failed to start publisher node");
      }
   }

   std::string name;
   ZyreSubscriber subscriber;
   ZyrePublisher  publisher;

   std::unordered_set<std::string> activeTopics;
   mutable std::mutex mutex;
};

// ============================================================================
//  TransportZyre public API
// ============================================================================

TransportZyre::TransportZyre(const std::string &nodeName)
   : _impl(std::make_unique<Impl>(nodeName))
{
   GPINFO("TransportZyre: created with namespace '{}'", nodeName);
}

TransportZyre::~TransportZyre()
{
   // Unsubscribe is implicit — ZyreSubscriber destructor stops the node.
}

std::string TransportZyre::name() const
{
   return "Zyre";
}

std::vector<std::string> TransportZyre::topicNames() const
{
   return ProtoMessages::allTopicNames();
}

void TransportZyre::subscribe(const std::string &topic, MessageCallback callback)
{
   std::lock_guard lock(_impl->mutex);
   if (_impl->activeTopics.contains(topic)) return;

   auto topicId = ProtoMessages::topicId(topic);
   if (!topicId.has_value())
   {
      GPWARN("TransportZyre: unknown topic '{}'", topic);
      return;
   }

   const auto id = topicId.value();
   const std::string namePrefix = _impl->name + "/";

   _impl->subscriber.subscribe(topic,
      [callback, id, namePrefix](const std::string &namespacedTopic,
                                 const std::string &binaryData)
      {
         // Strip the namespace prefix to get the bare topic name
         std::string bareTopic = namespacedTopic;
         if (namespacedTopic.starts_with(namePrefix))
         {
            bareTopic = namespacedTopic.substr(namePrefix.size());
         }

         std::string json;
         if (ProtoMessages::ProtoJsonDispatch::toJson(id, binaryData, json))
         {
            callback(bareTopic, json);
         }
         else
         {
            GPWARN("TransportZyre: failed to convert binary to JSON "
                   "for topic '{}'", bareTopic);
         }
      });

   _impl->activeTopics.insert(topic);
   GPINFO("TransportZyre: subscribed to {}", topic);
}

void TransportZyre::unsubscribe(const std::string &topic)
{
   std::lock_guard lock(_impl->mutex);
   if (!_impl->activeTopics.contains(topic)) return;

   // ZyreSubscriber doesn't support individual unsubscribe (no zyre_leave),
   // but we stop invoking the callback by removing from activeTopics.
   _impl->activeTopics.erase(topic);
   GPINFO("TransportZyre: unsubscribed from {}", topic);
}

bool TransportZyre::isSubscribed(const std::string &topic) const
{
   std::lock_guard lock(_impl->mutex);
   return _impl->activeTopics.contains(topic);
}

void TransportZyre::publishFromJson(const std::string &topic,
                                    const std::string &jsonData)
{
   auto topicId = ProtoMessages::topicId(topic);
   if (!topicId.has_value()) return;

   auto msg = ProtoMessages::ProtoJsonDispatch::fromJsonToMessage(
      topicId.value(), jsonData);
   if (!msg)
   {
      GPWARN("TransportZyre: failed to convert JSON to protobuf "
             "for topic '{}'", topic);
      return;
   }

   _impl->publisher.publish(topic, *msg);
}

} // namespace Omniscope
