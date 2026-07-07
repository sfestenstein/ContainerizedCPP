#ifndef VITA49CODEC_H_
#define VITA49CODEC_H_

#include "Vita49Types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Vita49_2
{

/**
 * @brief Result of parsing any single VITA 49.2 packet.
 */
struct ParsedPacket
{
   /// Discriminator for the parsed packet type
   enum class Type : uint8_t
   {
      SignalData,
      Context,
      Unknown
   };

   Type type{Type::Unknown};
   PacketHeader header;
   IQSamples samples;              ///< Populated for SignalData packets
   ContextFields contextFields;    ///< Populated for Context packets
};

/**
 * @brief High-level VITA 49.2 codec for encoding and decoding packet streams.
 *
 * This class wraps SignalDataPacket and ContextPacket to provide
 * convenient stream-oriented encoding/decoding. It handles:
 *   - Concatenated packet streams (multiple packets in one buffer)
 *   - Automatic packet splitting for large sample vectors
 *   - Configurable byte order and scale factor
 *
 * @note This is a pure codec — no networking. Pass raw byte buffers
 *       from HighBandwidthSubscriber / HighBandwidthPublisher.
 */
class Vita49Codec
{
public:
   /**
    * @brief Construct a VITA 49.2 codec.
    *
    * @param order Byte order for encode/decode (default: BigEndian per VITA standard)
    * @param scaleFactor Scale factor for int16/float conversion (default: 32768.0)
    */
   explicit Vita49Codec(ByteOrder order = ByteOrder::BigEndian,
                        float scaleFactor = DEFAULT_SCALE_FACTOR);

   // ========================================================================
   // Decoding
   // ========================================================================

   /**
    * @brief Parse a stream of concatenated VITA 49.2 packets.
    *
    * Iterates through the buffer, decoding packets one at a time
    * until the buffer is exhausted or a parse error occurs.
    *
    * @param data Pointer to the raw byte buffer
    * @param length Length of the buffer in bytes
    * @return Vector of parsed packets (may be empty on error)
    */
   [[nodiscard]] std::vector<ParsedPacket> parseStream(
      const uint8_t* data, size_t length) const;

   /**
    * @brief Parse a single VITA 49.2 packet from the buffer.
    *
    * @param data Pointer to the start of the packet
    * @param length Available bytes in the buffer
    * @param bytesConsumed [out] Number of bytes consumed
    * @return Parsed packet, or std::nullopt on error
    */
   [[nodiscard]] std::optional<ParsedPacket> parsePacket(
      const uint8_t* data, size_t length,
      size_t& bytesConsumed) const;

   // ========================================================================
   // Encoding
   // ========================================================================

   /**
    * @brief Encode I/Q samples as one or more Signal Data packets.
    *
    * If the samples exceed the maximum single-packet size, they are
    * automatically split into multiple packets with incrementing
    * packet counts (mod 16).
    *
    * @param streamId Stream identifier
    * @param samples I/Q samples to encode
    * @param startPacketCount Initial 4-bit sequence counter
    * @param tsiType Integer timestamp type (default: None)
    * @param tsfType Fractional timestamp type (default: None)
    * @param intTimestamp Integer timestamp for first packet
    * @param fracTimestamp Fractional timestamp for first packet
    * @param includeTrailer Whether to include trailer words
    * @return Concatenated packet bytes
    */
   [[nodiscard]] std::vector<uint8_t> encodeSignalData(
      uint32_t streamId,
      const IQSamples& samples,
      uint8_t startPacketCount = 0,
      TSI tsiType = TSI::None,
      TSF tsfType = TSF::None,
      uint32_t intTimestamp = 0,
      uint64_t fracTimestamp = 0,
      bool includeTrailer = false) const;

   /**
    * @brief Encode a Context packet.
    *
    * @param streamId Stream identifier
    * @param fields Context fields to include
    * @param packetCount 4-bit sequence counter
    * @param tsiType Integer timestamp type (default: None)
    * @param tsfType Fractional timestamp type (default: None)
    * @param intTimestamp Integer timestamp value
    * @param fracTimestamp Fractional timestamp value
    * @return Serialized packet bytes
    */
   [[nodiscard]] std::vector<uint8_t> encodeContext(
      uint32_t streamId,
      const ContextFields& fields,
      uint8_t packetCount = 0,
      TSI tsiType = TSI::None,
      TSF tsfType = TSF::None,
      uint32_t intTimestamp = 0,
      uint64_t fracTimestamp = 0) const;

   // ========================================================================
   // Configuration
   // ========================================================================

   void setByteOrder(ByteOrder order);
   [[nodiscard]] ByteOrder byteOrder() const;

   void setScaleFactor(float factor);
   [[nodiscard]] float scaleFactor() const;

private:
   ByteOrder _byteOrder;
   float _scaleFactor;
};

} // namespace Vita49_2

#endif // VITA49CODEC_H_
