#ifndef VITA49TYPES_H_
#define VITA49TYPES_H_

#include <complex>
#include <cstdint>
#include <optional>
#include <vector>

// NOLINTNEXTLINE - I like this namespace name, even though it goes against the usual style rules
namespace Vita49_2
{

// ============================================================================
// Enumerations
// ============================================================================

/// VITA 49.2 packet types
enum class PacketType : uint8_t
{
   IFDataWithoutStreamId  = 0x0,
   IFDataWithStreamId     = 0x1,
   ExtDataWithoutStreamId = 0x2,
   ExtDataWithStreamId    = 0x3,
   IFContext               = 0x4,
   ExtContext              = 0x5
};

/// Integer timestamp type (TSI field, header bits 23-22)
enum class TSI : uint8_t
{
   None  = 0x0,
   UTC   = 0x1,
   GPS   = 0x2,
   Other = 0x3
};

/// Fractional timestamp type (TSF field, header bits 21-20)
enum class TSF : uint8_t
{
   None        = 0x0,
   SampleCount = 0x1,
   RealTime    = 0x2,   ///< Picoseconds
   FreeRunning = 0x3
};

/// Byte order for packet serialization/deserialization
enum class ByteOrder : uint8_t
{
   BigEndian,      ///< Standard VITA 49 byte order
   LittleEndian    ///< Non-standard, for interop with LE systems
};

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Parsed VITA 49.2 packet header.
 *
 * Represents the common header fields present in all VITA 49.2 packets.
 * Optional fields are populated based on packet type and indicator bits.
 *
 * Header Word 0 layout (32 bits):
 *   Bits 31-28: Packet Type (4 bits)
 *   Bit 27:     C (Class ID present)
 *   Bits 26-24: Indicators (type-specific; bit 26 = Trailer for data packets)
 *   Bits 23-22: TSI (Integer Timestamp type)
 *   Bits 21-20: TSF (Fractional Timestamp type)
 *   Bits 19-16: Packet Count (4-bit sequence counter)
 *   Bits 15-0:  Packet Size (total size in 32-bit words)
 */
struct PacketHeader
{
   PacketType packetType{PacketType::IFDataWithStreamId};
   bool classIdPresent{false};
   bool trailerPresent{false};   ///< Data packets only (indicator bit 26)
   TSI tsiType{TSI::None};
   TSF tsfType{TSF::None};
   uint8_t packetCount{0};       ///< 4-bit sequence counter (0-15)
   uint16_t packetSize{0};       ///< Total packet size in 32-bit words

   std::optional<uint32_t> streamId;
   std::optional<uint32_t> classIdOUI;             ///< 24-bit OUI
   std::optional<uint16_t> informationClassCode;
   std::optional<uint16_t> packetClassCode;
   std::optional<uint32_t> integerTimestamp;
   std::optional<uint64_t> fractionalTimestamp;
};

/**
 * @brief Context fields for VITA 49.2 Context packets.
 *
 * Fields are identified by the Context Indicator Field (CIF0).
 * Only populated fields will be serialized; absent optionals
 * are omitted from the packet.
 */
struct ContextFields
{
   bool changeIndicator{false};                     ///< CIF0 bit 31 (flag only)
   std::optional<uint32_t> referencePointId;        ///< CIF0 bit 30, 1 word
   std::optional<double> bandwidth;                 ///< CIF0 bit 29, 2 words, Hz
   std::optional<double> ifRefFrequency;            ///< CIF0 bit 28, 2 words, Hz
   std::optional<double> rfFrequency;               ///< CIF0 bit 27, 2 words, Hz
   std::optional<double> rfFreqOffset;              ///< CIF0 bit 26, 2 words, Hz
   std::optional<double> ifBandOffset;              ///< CIF0 bit 25, 2 words, Hz
   std::optional<double> referenceLevel;            ///< CIF0 bit 24, 1 word, dBm
   std::optional<double> gain;                      ///< CIF0 bit 23, 1 word, dB
   std::optional<uint32_t> overRangeCount;          ///< CIF0 bit 22, 1 word
   std::optional<double> sampleRate;                ///< CIF0 bit 21, 2 words, Hz
};

// ============================================================================
// I/Q Sample Types
// ============================================================================

/// Single I/Q sample as complex float
using IQSample = std::complex<float>;

/// Vector of I/Q samples
using IQSamples = std::vector<IQSample>;

// ============================================================================
// Constants
// ============================================================================

/// Default scale factor for 16-bit to float conversion (full-scale = ±1.0)
constexpr float DEFAULT_SCALE_FACTOR = 32768.0f;

/// Maximum VITA 49 packet size in 32-bit words
constexpr uint16_t MAX_PACKET_SIZE_WORDS = 65535;

/// Maximum VITA 49 packet size in bytes
constexpr size_t MAX_PACKET_SIZE_BYTES = static_cast<size_t>(MAX_PACKET_SIZE_WORDS) * 4;

/// Radix point for 64-bit frequency/rate fields (20 fractional bits)
constexpr double FREQ_RADIX = 1048576.0;   // 2^20

/// Radix point for 16-bit gain/level fields (7 fractional bits)
constexpr double GAIN_RADIX = 128.0;       // 2^7

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if a packet type includes a Stream ID
[[nodiscard]] inline bool hasStreamId(PacketType type)
{
   switch (type)
   {
      case PacketType::IFDataWithStreamId:
      case PacketType::ExtDataWithStreamId:
      case PacketType::IFContext:
      case PacketType::ExtContext:
         return true;
      default:
         return false;
   }
}

/// Check if a packet type is a data packet (signal data)
[[nodiscard]] inline bool isDataPacket(PacketType type)
{
   return type == PacketType::IFDataWithoutStreamId ||
          type == PacketType::IFDataWithStreamId ||
          type == PacketType::ExtDataWithoutStreamId ||
          type == PacketType::ExtDataWithStreamId;
}

/// Check if a packet type is a context packet
[[nodiscard]] inline bool isContextPacket(PacketType type)
{
   return type == PacketType::IFContext ||
          type == PacketType::ExtContext;
}

} // namespace Vita49_2

#endif // VITA49TYPES_H_
