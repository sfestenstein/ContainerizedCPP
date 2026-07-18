#ifndef RAWSAMPLEJSONCODEC_H_
#define RAWSAMPLEJSONCODEC_H_

#include "DdsCore/DdsTypes.h"

#include <optional>
#include <string>

namespace Omniscope
{

/**
 * @brief Converts DdsCore::RawSample to/from OmniscopeDds's wire-JSON
 *        envelope shape, e.g.:
 *        {"raw_cdr":"<hex>","byte_count":N,"type_name":"...",
 *         "qos":{"reliability":"...","durability":"...","history_depth":N}}
 *
 * This is OmniscopeDds's own JSON protocol (sent over the WebSocket, and
 * stored in recordings) — a vendor-agnostic, DDS-library-free concern kept
 * separate from DdsCore's raw-bytes abstraction.
 */
namespace RawSampleJsonCodec
{

/// Encode a raw sample as the app's JSON message envelope.
[[nodiscard]] std::string toJson(const DdsCore::RawSample &sample);

/// Decode a raw sample from the app's JSON message envelope (as produced by
/// toJson() and stored in recordings). Returns std::nullopt if jsonData is
/// not valid JSON or is missing/malformed "raw_cdr".
[[nodiscard]] std::optional<DdsCore::RawSample> fromJson(const std::string &jsonData);

} // namespace RawSampleJsonCodec

} // namespace Omniscope

#endif // RAWSAMPLEJSONCODEC_H_
