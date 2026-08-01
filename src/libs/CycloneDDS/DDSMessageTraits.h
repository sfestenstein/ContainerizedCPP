#ifndef DDSMESSAGETRAITS_H_
#define DDSMESSAGETRAITS_H_

// System headers
#include <chrono>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace CycloneDDS
{

namespace detail
{

template <typename T, typename = void>
struct has_timestamp_header : std::false_type
{
};

template <typename T>
struct has_timestamp_header<
   T, std::void_t<decltype(std::declval<const T &>().header().timestamp_ns())>> : std::true_type
{
};

} // namespace detail

/**
 * @brief True if T has a `.header().timestamp_ns()` accessor chain (e.g.
 *        every RadarDDSDemo message type), false otherwise (e.g. the DDS
 *        test suite's TestMessage, which has no header).
 *
 * A compile-time trait, not a runtime check -- see extractLatencySinceSend().
 */
template <typename T>
constexpr bool has_timestamp_header_v = detail::has_timestamp_header<T>::value;

/**
 * @brief Compute the send-to-receive latency of a message, if it carries a
 *        header with a timestamp.
 *
 * Compiles out cleanly (always returns std::nullopt, no header field ever
 * accessed) for message types without `.header().timestamp_ns()` -- no IDL
 * changes are required for message types that don't opt in, and no
 * transport is forced to support latency.
 *
 * Uses std::chrono::system_clock, which is not synchronized across
 * processes/hosts in general; fine for a same-container demo where sender
 * and receiver share a clock, but a real cross-host deployment would need a
 * monotonic round-trip measurement or an NTP/PTP-synchronized clock
 * assumption instead.
 *
 * @return Elapsed time since the header's timestamp, or std::nullopt if T
 *         has no header, or if the computed latency would be negative
 *         (clock skew, or a malformed/replayed timestamp).
 */
template <typename T>
std::optional<std::chrono::nanoseconds> extractLatencySinceSend(const T &message)
{
   if constexpr (has_timestamp_header_v<T>)
   {
      int64_t sentNs = message.header().timestamp_ns();
      int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
      int64_t latencyNs = nowNs - sentNs;
      if (latencyNs < 0)
      {
         return std::nullopt;
      }
      return std::chrono::nanoseconds(latencyNs);
   }
   else
   {
      (void)message;
      return std::nullopt;
   }
}

} // namespace CycloneDDS

#endif // DDSMESSAGETRAITS_H_
