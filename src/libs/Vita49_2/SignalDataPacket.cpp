#include "SignalDataPacket.h"
#include "ByteSwap.h"
#include "PacketHeader.h"

#include <algorithm>
#include <cmath>

namespace Vita49_2
{

// ============================================================================
// decode
// ============================================================================

std::optional<SignalDataPacket::DecodeResult> SignalDataPacket::decode(
   const uint8_t* data, size_t length,
   ByteOrder order, float scaleFactor,
   size_t& bytesConsumed)
{
   if (data == nullptr || length < 4 || scaleFactor == 0.0f)
   {
      return std::nullopt;
   }

   // Parse header
   PacketHeader header;
   size_t headerBytes = 0;
   if (!PacketHeaderCodec::parse(data, length, order, header, headerBytes))
   {
      return std::nullopt;
   }

   // Must be a data packet
   if (!isDataPacket(header.packetType))
   {
      return std::nullopt;
   }

   const size_t packetBytes  = static_cast<size_t>(header.packetSize) * 4;
   const size_t trailerBytes = header.trailerPresent ? 4 : 0;

   if (headerBytes + trailerBytes > packetBytes)
   {
      return std::nullopt;
   }

   const size_t payloadBytes = packetBytes - headerBytes - trailerBytes;

   // Each I/Q pair = 4 bytes (16-bit I + 16-bit Q)
   const size_t numSamples = payloadBytes / 4;

   IQSamples samples;
   samples.reserve(numSamples);

   const uint8_t* payload = data + headerBytes;
   const float invScale = 1.0f / scaleFactor;

   for (size_t i = 0; i < numSamples; ++i)
   {
      const uint8_t* word = payload + (i * 4);
      int16_t iVal;
      int16_t qVal;

      if (order == ByteOrder::BigEndian)
      {
         iVal = readI16BE(word);
         qVal = readI16BE(word + 2);
      }
      else
      {
         iVal = readI16LE(word);
         qVal = readI16LE(word + 2);
      }

      samples.emplace_back(
         static_cast<float>(iVal) * invScale,
         static_cast<float>(qVal) * invScale);
   }

   bytesConsumed = packetBytes;

   DecodeResult result;
   result.header  = header;
   result.samples = std::move(samples);
   return result;
}

// ============================================================================
// encode
// ============================================================================

std::vector<uint8_t> SignalDataPacket::encode(
   uint32_t streamId,
   const IQSamples& samples,
   uint8_t packetCount,
   ByteOrder order,
   float scaleFactor,
   TSI tsiType,
   TSF tsfType,
   uint32_t intTimestamp,
   uint64_t fracTimestamp,
   bool includeTrailer)
{
   // Build header
   PacketHeader header;
   header.packetType     = PacketType::IFDataWithStreamId;
   header.classIdPresent = false;
   header.trailerPresent = includeTrailer;
   header.tsiType        = tsiType;
   header.tsfType        = tsfType;
   header.packetCount    = packetCount & 0xF;
   header.streamId       = streamId;

   if (tsiType != TSI::None)
   {
      header.integerTimestamp = intTimestamp;
   }
   if (tsfType != TSF::None)
   {
      header.fractionalTimestamp = fracTimestamp;
   }

   const size_t headerWords  = PacketHeaderCodec::sizeInWords(header);
   const size_t trailerWords = includeTrailer ? 1 : 0;
   const size_t payloadWords = samples.size();
   const size_t totalWords   = headerWords + payloadWords + trailerWords;

   if (totalWords > MAX_PACKET_SIZE_WORDS)
   {
      return {};
   }

   header.packetSize = static_cast<uint16_t>(totalWords);

   std::vector<uint8_t> out;
   out.reserve(totalWords * 4);

   // Serialize header
   PacketHeaderCodec::serialize(header, order, out);

   // Serialize I/Q payload
   for (const auto& sample : samples)
   {
      auto iRaw = static_cast<int>(std::lroundf(sample.real() * scaleFactor));
      auto qRaw = static_cast<int>(std::lroundf(sample.imag() * scaleFactor));
      auto i16  = static_cast<int16_t>(std::clamp(iRaw, -32768, 32767));
      auto q16  = static_cast<int16_t>(std::clamp(qRaw, -32768, 32767));

      uint8_t word[4];
      if (order == ByteOrder::BigEndian)
      {
         writeI16BE(word, i16);
         writeI16BE(word + 2, q16);
      }
      else
      {
         writeI16LE(word, i16);
         writeI16LE(word + 2, q16);
      }
      out.insert(out.end(), word, word + 4);
   }

   // Serialize trailer (all zeros)
   if (includeTrailer)
   {
      out.push_back(0);
      out.push_back(0);
      out.push_back(0);
      out.push_back(0);
   }

   return out;
}

// ============================================================================
// maxSamplesPerPacket
// ============================================================================

size_t SignalDataPacket::maxSamplesPerPacket(
   TSI tsiType, TSF tsfType,
   bool classIdPresent, bool includeTrailer)
{
   size_t headerWords = 2;  // word 0 + stream ID (IF Data With Stream ID)
   if (classIdPresent)    headerWords += 2;
   if (tsiType != TSI::None) headerWords += 1;
   if (tsfType != TSF::None) headerWords += 2;

   const size_t trailerWords = includeTrailer ? 1 : 0;
   const size_t overhead = headerWords + trailerWords;

   if (overhead >= MAX_PACKET_SIZE_WORDS)
   {
      return 0;
   }

   return MAX_PACKET_SIZE_WORDS - overhead;
}

} // namespace Vita49_2
