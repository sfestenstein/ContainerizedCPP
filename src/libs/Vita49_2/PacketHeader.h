#ifndef PACKETHEADER_H_
#define PACKETHEADER_H_

#include "Vita49Types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Vita49_2
{

/**
 * @brief Utility class for parsing and serializing VITA 49.2 packet headers.
 *
 * Handles the common header fields present in all VITA 49.2 packet types,
 * including the mandatory header word, optional Stream ID, Class ID,
 * and timestamp fields.
 */
class PacketHeaderCodec
{
public:
   /**
    * @brief Parse a VITA 49.2 packet header from a raw buffer.
    *
    * @param data Pointer to the start of the packet
    * @param length Available bytes in the buffer
    * @param order Byte order of the packet data
    * @param header [out] Parsed header fields
    * @param headerBytes [out] Number of bytes consumed by the header
    * @return true if the header was parsed successfully
    * @return false if the buffer is too small or the header is invalid
    */
   [[nodiscard]] static bool parse(const uint8_t* data, size_t length,
                                   ByteOrder order,
                                   PacketHeader& header, size_t& headerBytes);

   /**
    * @brief Serialize a VITA 49.2 packet header to a byte buffer.
    *
    * @param header The header to serialize (packetSize must already be set)
    * @param order Byte order for serialization
    * @param out [out] Bytes are appended to this vector
    */
   static void serialize(const PacketHeader& header, ByteOrder order,
                         std::vector<uint8_t>& out);

   /**
    * @brief Calculate the header size in 32-bit words.
    *
    * @param header The header to measure
    * @return Size in 32-bit words
    */
   [[nodiscard]] static size_t sizeInWords(const PacketHeader& header);

   /**
    * @brief Calculate the header size in bytes.
    *
    * @param header The header to measure
    * @return Size in bytes
    */
   [[nodiscard]] static size_t sizeInBytes(const PacketHeader& header);
};

} // namespace Vita49_2

#endif // PACKETHEADER_H_
