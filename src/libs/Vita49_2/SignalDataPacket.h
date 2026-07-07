#ifndef SIGNALDATAPACKET_H_
#define SIGNALDATAPACKET_H_

#include "Vita49Types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Vita49_2
{

/**
 * @brief Encodes and decodes VITA 49.2 Signal Data (IF Data) packets.
 *
 * Signal Data packets carry 16-bit signed I/Q sample pairs.
 * Each 32-bit payload word contains one I/Q pair:
 *   - Upper 16 bits: In-phase (I) component
 *   - Lower 16 bits: Quadrature (Q) component
 *
 * The scale factor controls the integer-to-float conversion:
 *   float_value = int16_value / scaleFactor
 */
class SignalDataPacket
{
public:
   /**
    * @brief Result of decoding a signal data packet.
    */
   struct DecodeResult
   {
      PacketHeader header;    ///< Parsed packet header
      IQSamples samples;      ///< Decoded I/Q samples
   };

   /**
    * @brief Decode a single Signal Data packet from a raw buffer.
    *
    * @param data Pointer to the start of the packet
    * @param length Available bytes in the buffer
    * @param order Byte order of the packet
    * @param scaleFactor Division factor for int16-to-float conversion
    * @param bytesConsumed [out] Number of bytes consumed by the packet
    * @return Decoded result, or std::nullopt on error
    */
   [[nodiscard]] static std::optional<DecodeResult> decode(
      const uint8_t* data, size_t length,
      ByteOrder order, float scaleFactor,
      size_t& bytesConsumed);

   /**
    * @brief Encode a single Signal Data packet.
    *
    * @param streamId Stream identifier
    * @param samples I/Q samples to encode
    * @param packetCount 4-bit sequence counter (0-15)
    * @param order Byte order for serialization
    * @param scaleFactor Multiplication factor for float-to-int16 conversion
    * @param tsiType Integer timestamp type (default: None)
    * @param tsfType Fractional timestamp type (default: None)
    * @param intTimestamp Integer timestamp value (used if tsiType != None)
    * @param fracTimestamp Fractional timestamp value (used if tsfType != None)
    * @param includeTrailer Whether to include a trailer word
    * @return Serialized packet bytes, or empty vector if samples exceed max
    */
   [[nodiscard]] static std::vector<uint8_t> encode(
      uint32_t streamId,
      const IQSamples& samples,
      uint8_t packetCount,
      ByteOrder order,
      float scaleFactor,
      TSI tsiType = TSI::None,
      TSF tsfType = TSF::None,
      uint32_t intTimestamp = 0,
      uint64_t fracTimestamp = 0,
      bool includeTrailer = false);

   /**
    * @brief Calculate the maximum number of I/Q samples that fit in one packet.
    *
    * @param tsiType Integer timestamp type
    * @param tsfType Fractional timestamp type
    * @param classIdPresent Whether Class ID is included
    * @param includeTrailer Whether trailer is included
    * @return Maximum sample count
    */
   [[nodiscard]] static size_t maxSamplesPerPacket(
      TSI tsiType = TSI::None,
      TSF tsfType = TSF::None,
      bool classIdPresent = false,
      bool includeTrailer = false);
};

} // namespace Vita49_2

#endif // SIGNALDATAPACKET_H_
