#ifndef DDSCORE_IRAWTOPICSUBSCRIBER_H_
#define DDSCORE_IRAWTOPICSUBSCRIBER_H_

#include "DdsCore/DdsTypes.h"

#include <cstdint>
#include <string>

namespace DdsCore
{

/**
 * @brief Vendor-agnostic subscription to raw (undecoded) samples on
 *        arbitrary, runtime-discovered topics.
 *
 * Implementations deliver samples as opaque bytes plus the metadata needed
 * to describe them — no compile-time knowledge of the payload type is
 * required, matching the needs of a generic topic snooper.
 */
class IRawTopicSubscriber
{
public:
   virtual ~IRawTopicSubscriber() = default;

   /// Begin receiving raw samples on the given topic. typeName/reliability/
   /// durability/historyDepth are stamped onto every RawSample delivered to
   /// callback, mirroring the discovered (or default) QoS for the topic.
   virtual void subscribe(const std::string &topic,
                          const std::string &typeName,
                          const std::string &reliability,
                          const std::string &durability,
                          int32_t historyDepth,
                          RawSampleCallback callback) = 0;

   /// Stop receiving samples on the given topic.
   virtual void unsubscribe(const std::string &topic) = 0;

   /// Returns true if currently subscribed to the given topic.
   [[nodiscard]] virtual bool isSubscribed(const std::string &topic) const = 0;
};

} // namespace DdsCore

#endif // DDSCORE_IRAWTOPICSUBSCRIBER_H_
