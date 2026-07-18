#ifndef DDSCORE_IRAWTOPICPUBLISHER_H_
#define DDSCORE_IRAWTOPICPUBLISHER_H_

#include "DdsCore/DdsTypes.h"

#include <string>

namespace DdsCore
{

/**
 * @brief Vendor-agnostic publication of raw (pre-serialized) samples onto
 *        arbitrary, runtime-named topics.
 *
 * Used for wire-level replay: the caller already has the exact bytes to
 * put on the wire (e.g. from a recording) and does not need general
 * value-to-wire encoding.
 */
class IRawTopicPublisher
{
public:
   virtual ~IRawTopicPublisher() = default;

   /// Publish a raw sample on the given topic, lazily creating whatever
   /// writer resources are needed. Returns false on failure.
   virtual bool publish(const std::string &topic, const RawSample &sample) = 0;
};

} // namespace DdsCore

#endif // DDSCORE_IRAWTOPICPUBLISHER_H_
