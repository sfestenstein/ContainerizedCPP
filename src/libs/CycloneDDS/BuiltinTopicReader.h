#ifndef BUILTINTOPICREADER_H_
#define BUILTINTOPICREADER_H_

#include "DdsCore/DdsTypes.h"

#include <dds/dds.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace CycloneDDS
{

enum class TopicEndpointRole
{
   Publisher,
   Subscriber,
};

/// Information about a topic discovered on the DDS network.
using DiscoveredTopic = DdsCore::DiscoveredTopic;

/// Invoked when a publisher endpoint appears (appeared=true) or leaves
/// the domain (appeared=false). The instance_handle uniquely identifies
/// the DataWriter — callers use it to track per-publisher presence.
using TopicDiscoveryCallback =
   std::function<void(const DiscoveredTopic & /*topic*/,
                      bool /*appeared*/,
                      TopicEndpointRole /*role*/,
                      dds_instance_handle_t /*endpointHandle*/)>
   ;

/**
 * @brief Watches the DCPSPublication built-in topic to discover all active
 *        topics and their QoS on a given DDS domain participant.
 *
 * Runs an internal polling thread. Invoke the destructor (or let the object
 * go out of scope) to stop it cleanly.
 */
class BuiltinTopicReader
{
public:
   /// @param participant  Existing C-API participant entity to attach the
   ///                     built-in reader to.
   /// @param builtinTopic  Built-in topic entity to read.
   /// @param callback     Called on the polling thread; must be thread-safe.
   BuiltinTopicReader(dds_entity_t participant, dds_entity_t builtinTopic,
                      TopicDiscoveryCallback callback);
   ~BuiltinTopicReader();

   BuiltinTopicReader(const BuiltinTopicReader &) = delete;
   BuiltinTopicReader &operator=(const BuiltinTopicReader &) = delete;

private:
   void pollLoop();

   dds_entity_t           _reader;
   TopicEndpointRole      _role;
   TopicDiscoveryCallback _callback;
   std::atomic<bool>      _running{true};
   std::thread            _thread;
};

} // namespace CycloneDDS

#endif // BUILTINTOPICREADER_H_
