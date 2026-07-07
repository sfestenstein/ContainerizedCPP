#include "ContextPacket.h"
#include "ByteSwap.h"
#include "PacketHeader.h"

#include <cmath>

namespace Vita49_2
{

namespace
{

// ============================================================================
// Byte-order-aware read/write helpers
// ============================================================================

uint32_t readWord(const uint8_t* p, ByteOrder order)
{
   return (order == ByteOrder::BigEndian) ? readU32BE(p) : readU32LE(p);
}

uint64_t readDWord(const uint8_t* p, ByteOrder order)
{
   return (order == ByteOrder::BigEndian) ? readU64BE(p) : readU64LE(p);
}

void appendWord(std::vector<uint8_t>& out, uint32_t val, ByteOrder order)
{
   uint8_t buf[4];
   if (order == ByteOrder::BigEndian)
      writeU32BE(buf, val);
   else
      writeU32LE(buf, val);
   out.insert(out.end(), buf, buf + 4);
}

void appendDWord(std::vector<uint8_t>& out, uint64_t val, ByteOrder order)
{
   uint8_t buf[8];
   if (order == ByteOrder::BigEndian)
      writeU64BE(buf, val);
   else
      writeU64LE(buf, val);
   out.insert(out.end(), buf, buf + 8);
}

// ============================================================================
// Fixed-point conversions
// ============================================================================

/// 64-bit frequency/rate: 20-bit radix (value / 2^20 = Hz)
double freqFromFixed(int64_t raw)
{
   return static_cast<double>(raw) / FREQ_RADIX;
}

int64_t freqToFixed(double hz)
{
   return static_cast<int64_t>(std::round(hz * FREQ_RADIX));
}

/// 32-bit reference level: signed 16-bit with 7 fractional bits in upper half
double refLevelFromFixed(uint32_t raw)
{
   auto val = static_cast<int16_t>(static_cast<uint16_t>((raw >> 16) & 0xFFFFu));
   return static_cast<double>(val) / GAIN_RADIX;
}

uint32_t refLevelToFixed(double dBm)
{
   auto val = static_cast<int16_t>(std::round(dBm * GAIN_RADIX));
   return static_cast<uint32_t>(static_cast<uint16_t>(val)) << 16;
}

/// 32-bit gain: Stage 1 (front-end) in lower 16 bits
double gainFromFixed(uint32_t raw)
{
   auto stage1 = static_cast<int16_t>(static_cast<uint16_t>(raw & 0xFFFFu));
   return static_cast<double>(stage1) / GAIN_RADIX;
}

uint32_t gainToFixed(double dB)
{
   auto val = static_cast<int16_t>(std::round(dB * GAIN_RADIX));
   return static_cast<uint32_t>(static_cast<uint16_t>(val));
}

// ============================================================================
// CIF0 bit positions
// ============================================================================

constexpr uint32_t CIF0_CHANGE_INDICATOR = (1u << 31);
constexpr uint32_t CIF0_REF_POINT_ID    = (1u << 30);
constexpr uint32_t CIF0_BANDWIDTH       = (1u << 29);
constexpr uint32_t CIF0_IF_REF_FREQ     = (1u << 28);
constexpr uint32_t CIF0_RF_REF_FREQ     = (1u << 27);
constexpr uint32_t CIF0_RF_FREQ_OFFSET  = (1u << 26);
constexpr uint32_t CIF0_IF_BAND_OFFSET  = (1u << 25);
constexpr uint32_t CIF0_REF_LEVEL       = (1u << 24);
constexpr uint32_t CIF0_GAIN            = (1u << 23);
constexpr uint32_t CIF0_OVER_RANGE      = (1u << 22);
constexpr uint32_t CIF0_SAMPLE_RATE     = (1u << 21);

/// Returns the size in 32-bit words of the data for a CIF0 bit position.
/// Returns 0 for flag-only bits, -1 for unknown/variable-sized fields.
int cif0FieldSizeWords(int bit)
{
   // NOLINTBEGIN - This is concise and expressive as is!
   switch (bit)
   {
      case 31: return 0;   // Change Indicator (flag only)
      case 30: return 1;   // Reference Point ID
      case 29: return 2;   // Bandwidth
      case 28: return 2;   // IF Reference Frequency
      case 27: return 2;   // RF Reference Frequency
      case 26: return 2;   // RF Reference Frequency Offset
      case 25: return 2;   // IF Band Offset
      case 24: return 1;   // Reference Level
      case 23: return 1;   // Gain/Attenuation
      case 22: return 1;   // Over-Range Count
      case 21: return 2;   // Sample Rate
      case 20: return 2;   // Timestamp Adjustment
      case 19: return 1;   // Timestamp Calibration Time
      case 18: return 1;   // Temperature
      case 17: return 2;   // Device Identifier
      case 16: return 1;   // State and Event Indicators
      case 15: return 2;   // Data Packet Payload Format
      default: return -1;  // Unknown / variable size
   }
   // NOLINTEND
}

} // anonymous namespace

// ============================================================================
// decode
// ============================================================================

std::optional<ContextPacket::DecodeResult> ContextPacket::decode(
   const uint8_t* data, size_t length,
   ByteOrder order,
   size_t& bytesConsumed)
{
   if (data == nullptr || length < 4)
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

   // Must be a context packet
   if (!isContextPacket(header.packetType))
   {
      return std::nullopt;
   }

   const size_t packetBytes = static_cast<size_t>(header.packetSize) * 4;
   size_t offset = headerBytes;

   // Need at least CIF0 (1 word)
   if (offset + 4 > packetBytes)
   {
      return std::nullopt;
   }

   const uint32_t cif0 = readWord(data + offset, order);
   offset += 4;

   ContextFields fields;
   fields.changeIndicator = (cif0 & CIF0_CHANGE_INDICATOR) != 0;

   // Iterate CIF0 bits from 30 down to 15 (all fixed-size fields)
   for (int bit = 30; bit >= 15; --bit)
   {
      const uint32_t mask = (1u << static_cast<unsigned>(bit));
      if ((cif0 & mask) == 0)
      {
         continue;
      }

      const int sizeWords = cif0FieldSizeWords(bit);
      if (sizeWords < 0)
      {
         break;   // Unknown field size — stop parsing
      }
      if (sizeWords == 0)
      {
         continue; // Flag only, no data
      }

      const size_t fieldBytes = static_cast<size_t>(sizeWords) * 4;
      if (offset + fieldBytes > packetBytes)
      {
         return std::nullopt;   // Truncated packet
      }

      // Decode supported fields; skip unsupported ones
      switch (bit)
      {
         case 30:
            fields.referencePointId = readWord(data + offset, order);
            break;
         case 29:
         {
            auto raw = static_cast<int64_t>(readDWord(data + offset, order));
            fields.bandwidth = freqFromFixed(raw);
            break;
         }
         case 28:
         {
            auto raw = static_cast<int64_t>(readDWord(data + offset, order));
            fields.ifRefFrequency = freqFromFixed(raw);
            break;
         }
         case 27:
         {
            auto raw = static_cast<int64_t>(readDWord(data + offset, order));
            fields.rfFrequency = freqFromFixed(raw);
            break;
         }
         case 26:
         {
            auto raw = static_cast<int64_t>(readDWord(data + offset, order));
            fields.rfFreqOffset = freqFromFixed(raw);
            break;
         }
         case 25:
         {
            auto raw = static_cast<int64_t>(readDWord(data + offset, order));
            fields.ifBandOffset = freqFromFixed(raw);
            break;
         }
         case 24:
         {
            const uint32_t raw = readWord(data + offset, order);
            fields.referenceLevel = refLevelFromFixed(raw);
            break;
         }
         case 23:
         {
            const uint32_t raw = readWord(data + offset, order);
            fields.gain = gainFromFixed(raw);
            break;
         }
         case 22:
            fields.overRangeCount = readWord(data + offset, order);
            break;
         case 21:
         {
            auto raw = static_cast<int64_t>(readDWord(data + offset, order));
            fields.sampleRate = freqFromFixed(raw);
            break;
         }
         default:
            // Skip unsupported field (bits 20-15)
            break;
      }

      offset += fieldBytes;
   }

   bytesConsumed = packetBytes;

   DecodeResult result;
   result.header = header;
   result.fields = fields;
   return result;
}

// ============================================================================
// encode
// ============================================================================

std::vector<uint8_t> ContextPacket::encode(
   uint32_t streamId,
   const ContextFields& fields,
   uint8_t packetCount,
   ByteOrder order,
   TSI tsiType,
   TSF tsfType,
   uint32_t intTimestamp,
   uint64_t fracTimestamp)
{
   // Build CIF0 and serialized field data in MSB-first order
   uint32_t cif0 = 0;
   std::vector<uint8_t> fieldData;

   if (fields.changeIndicator)
   {
      cif0 |= CIF0_CHANGE_INDICATOR;
   }

   if (fields.referencePointId.has_value())
   {
      cif0 |= CIF0_REF_POINT_ID;
      appendWord(fieldData, fields.referencePointId.value(), order);
   }

   if (fields.bandwidth.has_value())
   {
      cif0 |= CIF0_BANDWIDTH;
      auto fixed = static_cast<uint64_t>(freqToFixed(fields.bandwidth.value()));
      appendDWord(fieldData, fixed, order);
   }

   if (fields.ifRefFrequency.has_value())
   {
      cif0 |= CIF0_IF_REF_FREQ;
      auto fixed = static_cast<uint64_t>(freqToFixed(fields.ifRefFrequency.value()));
      appendDWord(fieldData, fixed, order);
   }

   if (fields.rfFrequency.has_value())
   {
      cif0 |= CIF0_RF_REF_FREQ;
      auto fixed = static_cast<uint64_t>(freqToFixed(fields.rfFrequency.value()));
      appendDWord(fieldData, fixed, order);
   }

   if (fields.rfFreqOffset.has_value())
   {
      cif0 |= CIF0_RF_FREQ_OFFSET;
      auto fixed = static_cast<uint64_t>(freqToFixed(fields.rfFreqOffset.value()));
      appendDWord(fieldData, fixed, order);
   }

   if (fields.ifBandOffset.has_value())
   {
      cif0 |= CIF0_IF_BAND_OFFSET;
      auto fixed = static_cast<uint64_t>(freqToFixed(fields.ifBandOffset.value()));
      appendDWord(fieldData, fixed, order);
   }

   if (fields.referenceLevel.has_value())
   {
      cif0 |= CIF0_REF_LEVEL;
      appendWord(fieldData, refLevelToFixed(fields.referenceLevel.value()), order);
   }

   if (fields.gain.has_value())
   {
      cif0 |= CIF0_GAIN;
      appendWord(fieldData, gainToFixed(fields.gain.value()), order);
   }

   if (fields.overRangeCount.has_value())
   {
      cif0 |= CIF0_OVER_RANGE;
      appendWord(fieldData, fields.overRangeCount.value(), order);
   }

   if (fields.sampleRate.has_value())
   {
      cif0 |= CIF0_SAMPLE_RATE;
      auto fixed = static_cast<uint64_t>(freqToFixed(fields.sampleRate.value()));
      appendDWord(fieldData, fixed, order);
   }

   // Build header
   PacketHeader header;
   header.packetType     = PacketType::IFContext;
   header.classIdPresent = false;
   header.trailerPresent = false;
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

   const size_t headerWords = PacketHeaderCodec::sizeInWords(header);
   const size_t cif0Words   = 1;
   const size_t fieldWords  = fieldData.size() / 4;
   const size_t totalWords  = headerWords + cif0Words + fieldWords;

   if (totalWords > MAX_PACKET_SIZE_WORDS)
   {
      return {};
   }

   header.packetSize = static_cast<uint16_t>(totalWords);

   std::vector<uint8_t> out;
   out.reserve(totalWords * 4);

   // Serialize: header → CIF0 → field data
   PacketHeaderCodec::serialize(header, order, out);
   appendWord(out, cif0, order);
   out.insert(out.end(), fieldData.begin(), fieldData.end());

   return out;
}

} // namespace Vita49_2
