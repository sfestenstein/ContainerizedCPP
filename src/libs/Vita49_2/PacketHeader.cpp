#include "PacketHeader.h"
#include "ByteSwap.h"

namespace Vita49_2
{

namespace
{

uint32_t readWord(const uint8_t* p, ByteOrder order)
{
   return (order == ByteOrder::BigEndian) ? readU32BE(p) : readU32LE(p);
}

uint64_t readDWord(const uint8_t* p, ByteOrder order)
{
   return (order == ByteOrder::BigEndian) ? readU64BE(p) : readU64LE(p);
}

void writeWord(uint8_t* p, uint32_t val, ByteOrder order)
{
   if (order == ByteOrder::BigEndian)
      writeU32BE(p, val);
   else
      writeU32LE(p, val);
}

void writeDWord(uint8_t* p, uint64_t val, ByteOrder order)
{
   if (order == ByteOrder::BigEndian)
      writeU64BE(p, val);
   else
      writeU64LE(p, val);
}

} // anonymous namespace

// ============================================================================
// parse
// ============================================================================

bool PacketHeaderCodec::parse(const uint8_t* data, size_t length,
                              ByteOrder order,
                              PacketHeader& header, size_t& headerBytes)
{
   if (data == nullptr || length < 4)
   {
      return false;
   }

   // ---- Word 0 ----
   const uint32_t word0 = readWord(data, order);

   header.packetType     = static_cast<PacketType>((word0 >> 28) & 0xFu);
   header.classIdPresent = ((word0 >> 27) & 0x1u) != 0;
   header.trailerPresent = isDataPacket(header.packetType) &&
                           (((word0 >> 26) & 0x1u) != 0);
   header.tsiType        = static_cast<TSI>((word0 >> 22) & 0x3u);
   header.tsfType        = static_cast<TSF>((word0 >> 20) & 0x3u);
   header.packetCount    = static_cast<uint8_t>((word0 >> 16) & 0xFu);
   header.packetSize     = static_cast<uint16_t>(word0 & 0xFFFFu);

   // Validate packet size
   if (header.packetSize == 0 || header.packetSize > MAX_PACKET_SIZE_WORDS)
   {
      return false;
   }

   const size_t packetBytes = static_cast<size_t>(header.packetSize) * 4;
   if (length < packetBytes)
   {
      return false;
   }

   size_t offset = 4;

   // ---- Stream ID (types 1, 3, 4, 5) ----
   if (hasStreamId(header.packetType))
   {
      if (offset + 4 > packetBytes) return false;
      header.streamId = readWord(data + offset, order);
      offset += 4;
   }

   // ---- Class ID (2 words) ----
   if (header.classIdPresent)
   {
      if (offset + 8 > packetBytes) return false;
      const uint32_t classWord1 = readWord(data + offset, order);
      const uint32_t classWord2 = readWord(data + offset + 4, order);
      header.classIdOUI             = classWord1 & 0x00FFFFFFu;
      header.informationClassCode   = static_cast<uint16_t>((classWord2 >> 16) & 0xFFFFu);
      header.packetClassCode        = static_cast<uint16_t>(classWord2 & 0xFFFFu);
      offset += 8;
   }

   // ---- Integer Timestamp (1 word) ----
   if (header.tsiType != TSI::None)
   {
      if (offset + 4 > packetBytes) return false;
      header.integerTimestamp = readWord(data + offset, order);
      offset += 4;
   }

   // ---- Fractional Timestamp (2 words / 64 bits) ----
   if (header.tsfType != TSF::None)
   {
      if (offset + 8 > packetBytes) return false;
      header.fractionalTimestamp = readDWord(data + offset, order);
      offset += 8;
   }

   headerBytes = offset;
   return true;
}

// ============================================================================
// serialize
// ============================================================================

void PacketHeaderCodec::serialize(const PacketHeader& header, ByteOrder order,
                                  std::vector<uint8_t>& out)
{
   // ---- Build Word 0 ----
   uint32_t word0 = 0;
   word0 |= (static_cast<uint32_t>(header.packetType) & 0xFu) << 28;
   word0 |= (header.classIdPresent ? 1u : 0u) << 27;

   if (isDataPacket(header.packetType))
   {
      word0 |= (header.trailerPresent ? 1u : 0u) << 26;
   }

   word0 |= (static_cast<uint32_t>(header.tsiType) & 0x3u) << 22;
   word0 |= (static_cast<uint32_t>(header.tsfType) & 0x3u) << 20;
   word0 |= (static_cast<uint32_t>(header.packetCount) & 0xFu) << 16;
   word0 |= static_cast<uint32_t>(header.packetSize) & 0xFFFFu;

   size_t pos = out.size();
   out.resize(pos + 4);
   writeWord(out.data() + pos, word0, order);

   // ---- Stream ID ----
   if (hasStreamId(header.packetType) && header.streamId.has_value())
   {
      pos = out.size();
      out.resize(pos + 4);
      writeWord(out.data() + pos, header.streamId.value(), order);
   }

   // ---- Class ID (2 words) ----
   if (header.classIdPresent && header.classIdOUI.has_value())
   {
      pos = out.size();
      out.resize(pos + 8);
      const uint32_t classWord1 = header.classIdOUI.value() & 0x00FFFFFFu;
      uint32_t classWord2 = 0;
      if (header.informationClassCode.has_value())
         classWord2 |= static_cast<uint32_t>(header.informationClassCode.value()) << 16;
      if (header.packetClassCode.has_value())
         classWord2 |= static_cast<uint32_t>(header.packetClassCode.value());
      writeWord(out.data() + pos, classWord1, order);
      writeWord(out.data() + pos + 4, classWord2, order);
   }

   // ---- Integer Timestamp ----
   if (header.tsiType != TSI::None && header.integerTimestamp.has_value())
   {
      pos = out.size();
      out.resize(pos + 4);
      writeWord(out.data() + pos, header.integerTimestamp.value(), order);
   }

   // ---- Fractional Timestamp (64-bit) ----
   if (header.tsfType != TSF::None && header.fractionalTimestamp.has_value())
   {
      pos = out.size();
      out.resize(pos + 8);
      writeDWord(out.data() + pos, header.fractionalTimestamp.value(), order);
   }
}

// ============================================================================
// sizeInWords / sizeInBytes
// ============================================================================

size_t PacketHeaderCodec::sizeInWords(const PacketHeader& header)
{
   size_t words = 1; // Word 0
   if (hasStreamId(header.packetType))  words += 1;
   if (header.classIdPresent)           words += 2;
   if (header.tsiType != TSI::None)     words += 1;
   if (header.tsfType != TSF::None)     words += 2;
   return words;
}

size_t PacketHeaderCodec::sizeInBytes(const PacketHeader& header)
{
   return sizeInWords(header) * 4;
}

} // namespace Vita49_2
