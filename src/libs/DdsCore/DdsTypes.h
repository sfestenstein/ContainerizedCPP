#ifndef DDSCORE_DDSTYPES_H_
#define DDSCORE_DDSTYPES_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace DdsCore
{

/// Information about a topic discovered on the DDS network. Vendor-neutral:
/// reliability/durability are free-form strings ("reliable" | "best_effort",
/// "volatile" | "transient_local" | "transient" | "persistent") rather than
/// any vendor's QoS enum, so this type carries no vendor SDK dependency.
struct DiscoveredTopic
{
   std::string name;
   std::string typeName;
   std::string reliability;
   std::string durability;
   int32_t     historyDepth; // -1 = KEEP_ALL
};

/// A single sample's raw wire bytes plus enough metadata to reconstruct a
/// compatible reader/writer for it, without ever decoding the payload.
struct RawSample
{
   std::string          typeName;
   std::vector<uint8_t> bytes;
   std::string          reliability;
   std::string          durability;
   int32_t              historyDepth;
};

/// Invoked whenever the set of known topics changes (topics appear or
/// disappear).
using TopicsChangedCallback = std::function<void()>;

/// Invoked for each raw sample received on a subscribed topic.
using RawSampleCallback = std::function<void(const std::string &topic, const RawSample &sample)>;

} // namespace DdsCore

#endif // DDSCORE_DDSTYPES_H_
