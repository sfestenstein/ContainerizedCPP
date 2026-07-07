#ifndef CONTEXTPACKET_H_
#define CONTEXTPACKET_H_

#include "Vita49Types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Vita49_2
{

/**
 * @brief Encodes and decodes VITA 49.2 IF Context packets.
 *
 * Context packets carry metadata about the signal data stream,
 * such as sample rate, RF frequency, bandwidth, gain, etc.
 *
 * Fields are identified by the Context Indicator Field (CIF0),
 * a 32-bit bitmask where each bit indicates the presence of a
 * specific context field in the packet body.
 *
 * Supported CIF0 fields (bits 31-21):
 *   Bit 31: Change Indicator (flag only, no data)
 *   Bit 30: Reference Point ID (1 word)
 *   Bit 29: Bandwidth (2 words, 64-bit fixed-point Hz)
 *   Bit 28: IF Reference Frequency (2 words)
 *   Bit 27: RF Reference Frequency (2 words)
 *   Bit 26: RF Frequency Offset (2 words)
 *   Bit 25: IF Band Offset (2 words)
 *   Bit 24: Reference Level (1 word, 16-bit fixed-point dBm)
 *   Bit 23: Gain/Attenuation (1 word, 16-bit fixed-point dB)
 *   Bit 22: Over-Range Count (1 word)
 *   Bit 21: Sample Rate (2 words, 64-bit fixed-point Hz)
 *
 * Fields at bits 20-8 are skipped during decode if present.
 */
class ContextPacket
{
public:
   /**
    * @brief Result of decoding a context packet.
    */
   struct DecodeResult
   {
      PacketHeader header;    ///< Parsed packet header
      ContextFields fields;   ///< Decoded context fields
   };

   /**
    * @brief Decode a single Context packet from a raw buffer.
    *
    * @param data Pointer to the start of the packet
    * @param length Available bytes in the buffer
    * @param order Byte order of the packet
    * @param bytesConsumed [out] Number of bytes consumed by the packet
    * @return Decoded result, or std::nullopt on error
    */
   [[nodiscard]] static std::optional<DecodeResult> decode(
      const uint8_t* data, size_t length,
      ByteOrder order,
      size_t& bytesConsumed);

   /**
    * @brief Encode a single Context packet.
    *
    * @param streamId Stream identifier
    * @param fields Context fields to include
    * @param packetCount 4-bit sequence counter (0-15)
    * @param order Byte order for serialization
    * @param tsiType Integer timestamp type (default: None)
    * @param tsfType Fractional timestamp type (default: None)
    * @param intTimestamp Integer timestamp value (used if tsiType != None)
    * @param fracTimestamp Fractional timestamp value (used if tsfType != None)
    * @return Serialized packet bytes
    */
   [[nodiscard]] static std::vector<uint8_t> encode(
      uint32_t streamId,
      const ContextFields& fields,
      uint8_t packetCount,
      ByteOrder order,
      TSI tsiType = TSI::None,
      TSF tsfType = TSF::None,
      uint32_t intTimestamp = 0,
      uint64_t fracTimestamp = 0);
};

} // namespace Vita49_2

#endif // CONTEXTPACKET_H_
