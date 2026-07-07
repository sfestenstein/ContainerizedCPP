#include "Vita49Codec.h"
#include "ContextPacket.h"
#include "SignalDataPacket.h"

#include <algorithm>

namespace Vita49_2
{

// ============================================================================
// Constructor
// ============================================================================

Vita49Codec::Vita49Codec(ByteOrder order, float scaleFactor)
   : _byteOrder(order)
   , _scaleFactor(scaleFactor)
{
}

// ============================================================================
// Decoding
// ============================================================================

std::vector<ParsedPacket> Vita49Codec::parseStream(
   const uint8_t* data, size_t length) const
{
   std::vector<ParsedPacket> results;

   if (data == nullptr || length == 0)
   {
      return results;
   }

   size_t offset = 0;
   while (offset < length)
   {
      const size_t remaining = length - offset;
      if (remaining < 4)
      {
         break;   // Not enough data for a header word
      }

      size_t consumed = 0;
      auto packet = parsePacket(data + offset, remaining, consumed);
      if (!packet.has_value() || consumed == 0)
      {
         break;   // Parse error or zero progress
      }

      results.push_back(std::move(packet.value()));
      offset += consumed;
   }

   return results;
}

std::optional<ParsedPacket> Vita49Codec::parsePacket(
   const uint8_t* data, size_t length,
   size_t& bytesConsumed) const
{
   if (data == nullptr || length < 4)
   {
      return std::nullopt;
   }

   // Peek at packet type from word 0
   uint32_t word0;
   if (_byteOrder == ByteOrder::BigEndian)
   {
      word0 = (static_cast<uint32_t>(data[0]) << 24) |
              (static_cast<uint32_t>(data[1]) << 16) |
              (static_cast<uint32_t>(data[2]) << 8)  |
               static_cast<uint32_t>(data[3]);
   }
   else
   {
      word0 =  static_cast<uint32_t>(data[0])        |
              (static_cast<uint32_t>(data[1]) << 8)  |
              (static_cast<uint32_t>(data[2]) << 16) |
              (static_cast<uint32_t>(data[3]) << 24);
   }

   auto packetType = static_cast<PacketType>((word0 >> 28) & 0xFu);

   ParsedPacket result;

   if (isDataPacket(packetType))
   {
      auto decoded = SignalDataPacket::decode(
         data, length, _byteOrder, _scaleFactor, bytesConsumed);

      if (!decoded.has_value())
      {
         return std::nullopt;
      }

      result.type    = ParsedPacket::Type::SignalData;
      result.header  = decoded->header;
      result.samples = std::move(decoded->samples);
   }
   else if (isContextPacket(packetType))
   {
      auto decoded = ContextPacket::decode(
         data, length, _byteOrder, bytesConsumed);

      if (!decoded.has_value())
      {
         return std::nullopt;
      }

      result.type          = ParsedPacket::Type::Context;
      result.header        = decoded->header;
      result.contextFields = decoded->fields;
   }
   else
   {
      // Unknown packet type — skip by reading packet size from word 0
      auto packetSize = static_cast<uint16_t>(word0 & 0xFFFFu);
      const size_t packetBytes = static_cast<size_t>(packetSize) * 4;
      if (packetSize == 0 || packetBytes > length)
      {
         return std::nullopt;
      }

      result.type = ParsedPacket::Type::Unknown;
      bytesConsumed = packetBytes;
   }

   return result;
}

// ============================================================================
// Encoding
// ============================================================================

std::vector<uint8_t> Vita49Codec::encodeSignalData(
   uint32_t streamId,
   const IQSamples& samples,
   uint8_t startPacketCount,
   TSI tsiType,
   TSF tsfType,
   uint32_t intTimestamp,
   uint64_t fracTimestamp,
   bool includeTrailer) const
{
   const size_t maxPerPacket = SignalDataPacket::maxSamplesPerPacket(
      tsiType, tsfType, false, includeTrailer);

   if (maxPerPacket == 0)
   {
      return {};
   }

   std::vector<uint8_t> result;

   size_t offset      = 0;
   uint8_t pktCount   = startPacketCount;

   while (offset < samples.size())
   {
      const size_t count = std::min(maxPerPacket, samples.size() - offset);

      const IQSamples chunk(
         samples.begin() + static_cast<ptrdiff_t>(offset),
         samples.begin() + static_cast<ptrdiff_t>(offset + count));

      auto packet = SignalDataPacket::encode(
         streamId, chunk, pktCount, _byteOrder, _scaleFactor,
         tsiType, tsfType, intTimestamp, fracTimestamp, includeTrailer);

      if (packet.empty())
      {
         break;
      }

      result.insert(result.end(), packet.begin(), packet.end());
      offset   += count;
      pktCount  = (pktCount + 1) & 0xF;
   }

   // Handle the empty-samples case: produce one packet with no payload
   if (samples.empty())
   {
      auto packet = SignalDataPacket::encode(
         streamId, samples, startPacketCount, _byteOrder, _scaleFactor,
         tsiType, tsfType, intTimestamp, fracTimestamp, includeTrailer);
      return packet;
   }

   return result;
}

std::vector<uint8_t> Vita49Codec::encodeContext(
   uint32_t streamId,
   const ContextFields& fields,
   uint8_t packetCount,
   TSI tsiType,
   TSF tsfType,
   uint32_t intTimestamp,
   uint64_t fracTimestamp) const
{
   return ContextPacket::encode(
      streamId, fields, packetCount, _byteOrder,
      tsiType, tsfType, intTimestamp, fracTimestamp);
}

// ============================================================================
// Configuration
// ============================================================================

void Vita49Codec::setByteOrder(ByteOrder order)
{
   _byteOrder = order;
}

ByteOrder Vita49Codec::byteOrder() const
{
   return _byteOrder;
}

void Vita49Codec::setScaleFactor(float factor)
{
   _scaleFactor = factor;
}

float Vita49Codec::scaleFactor() const
{
   return _scaleFactor;
}

} // namespace Vita49_2
