/**
 * @file ContextPacketUt.cpp
 * @brief Unit tests for Vita49_2::ContextPacket.
 */

#include <gtest/gtest.h>
#include "ContextPacket.h"
#include "ByteSwap.h"

#include <cmath>
#include <vector>

using namespace Vita49_2;

// 64-bit frequency precision: 1 LSB ≈ 0.95 µHz
static constexpr double FREQ_TOL = 1.0e-6;
// 16-bit gain precision: 1 LSB ≈ 0.0078125 dB
static constexpr double GAIN_TOL = 0.008;

// ============================================================================
// Decode — Known Byte Patterns
// ============================================================================

TEST(ContextPacketTest, Decode_SampleRateOnly_BigEndian)
{
   // header(1) + streamId(1) + CIF0(1) + sampleRate(2) = 5 words
   uint8_t data[20];
   writeU32BE(data, 0x40000005);
   writeU32BE(data + 4, 0x12345678);
   writeU32BE(data + 8, 0x00200000);   // CIF0 bit 21 = sample rate

   // 1,000,000 Hz × 2^20 = 1,048,576,000,000 = 0x000000F424000000
   writeU64BE(data + 12, 0x000000F424000000ull);

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(consumed, 20u);
   EXPECT_EQ(result->header.packetType, PacketType::IFContext);
   ASSERT_TRUE(result->header.streamId.has_value());
   EXPECT_EQ(result->header.streamId.value(), 0x12345678u);
   ASSERT_TRUE(result->fields.sampleRate.has_value());
   EXPECT_NEAR(result->fields.sampleRate.value(), 1000000.0, FREQ_TOL);
}

TEST(ContextPacketTest, Decode_MultipleFields_BigEndian)
{
   // BW(bit 29) + RF(bit 27) + SR(bit 21)
   // CIF0 = 0x20000000 | 0x08000000 | 0x00200000 = 0x28200000
   // Body: BW(2) + RF(2) + SR(2) = 6 words
   // Total: 1 + 1 + 1 + 6 = 9 words
   uint8_t data[36];
   writeU32BE(data, 0x40000009);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x28200000);

   auto bwFixed  = static_cast<uint64_t>(
      static_cast<int64_t>(10000000.0 * FREQ_RADIX));
   auto rfFixed  = static_cast<uint64_t>(
      static_cast<int64_t>(915000000.0 * FREQ_RADIX));
   auto srFixed  = static_cast<uint64_t>(
      static_cast<int64_t>(20000000.0 * FREQ_RADIX));

   writeU64BE(data + 12, bwFixed);
   writeU64BE(data + 20, rfFixed);
   writeU64BE(data + 28, srFixed);

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_TRUE(result->fields.bandwidth.has_value());
   EXPECT_NEAR(result->fields.bandwidth.value(), 10000000.0, FREQ_TOL);
   ASSERT_TRUE(result->fields.rfFrequency.has_value());
   EXPECT_NEAR(result->fields.rfFrequency.value(), 915000000.0, FREQ_TOL);
   ASSERT_TRUE(result->fields.sampleRate.has_value());
   EXPECT_NEAR(result->fields.sampleRate.value(), 20000000.0, FREQ_TOL);
}

TEST(ContextPacketTest, Decode_GainAndRefLevel_BigEndian)
{
   // CIF0 bits 24 + 23 = 0x01800000
   // Fields: RefLevel(1) + Gain(1)
   // Total: 1 + 1 + 1 + 2 = 5 words
   uint8_t data[20];
   writeU32BE(data, 0x40000005);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x01800000);

   // Ref level = -30.0 dBm → fixed = round(-30 × 128) = -3840 (int16)
   auto refFixed = static_cast<int16_t>(std::round(-30.0 * GAIN_RADIX));
   writeU32BE(data + 12,
      static_cast<uint32_t>(static_cast<uint16_t>(refFixed)) << 16);

   // Gain = 15.5 dB → fixed = round(15.5 × 128) = 1984 (int16)
   auto gainFixed = static_cast<int16_t>(std::round(15.5 * GAIN_RADIX));
   writeU32BE(data + 16,
      static_cast<uint32_t>(static_cast<uint16_t>(gainFixed)));

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_TRUE(result->fields.referenceLevel.has_value());
   EXPECT_NEAR(result->fields.referenceLevel.value(), -30.0, GAIN_TOL);
   ASSERT_TRUE(result->fields.gain.has_value());
   EXPECT_NEAR(result->fields.gain.value(), 15.5, GAIN_TOL);
}

TEST(ContextPacketTest, Decode_OverRangeCount)
{
   // CIF0 bit 22 = 0x00400000
   uint8_t data[16];
   writeU32BE(data, 0x40000004);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x00400000);
   writeU32BE(data + 12, 42);

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_TRUE(result->fields.overRangeCount.has_value());
   EXPECT_EQ(result->fields.overRangeCount.value(), 42u);
}

TEST(ContextPacketTest, Decode_ReferencePointId)
{
   // CIF0 bit 30 = 0x40000000
   uint8_t data[16];
   writeU32BE(data, 0x40000004);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x40000000);
   writeU32BE(data + 12, 0xABCD0001);

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_TRUE(result->fields.referencePointId.has_value());
   EXPECT_EQ(result->fields.referencePointId.value(), 0xABCD0001u);
}

TEST(ContextPacketTest, Decode_ChangeIndicator)
{
   uint8_t data[12];
   writeU32BE(data, 0x40000003);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x80000000);  // CIF0 bit 31

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(result.has_value());
   EXPECT_TRUE(result->fields.changeIndicator);
}

TEST(ContextPacketTest, Decode_EmptyCIF0)
{
   uint8_t data[12];
   writeU32BE(data, 0x40000003);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x00000000);

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(result.has_value());
   EXPECT_FALSE(result->fields.changeIndicator);
   EXPECT_FALSE(result->fields.sampleRate.has_value());
   EXPECT_FALSE(result->fields.bandwidth.has_value());
   EXPECT_FALSE(result->fields.rfFrequency.has_value());
}

TEST(ContextPacketTest, Decode_LittleEndian)
{
   uint8_t data[20];
   writeU32LE(data, 0x40000005);
   writeU32LE(data + 4, 0x00000001);
   writeU32LE(data + 8, 0x00200000);

   auto srFixed = static_cast<uint64_t>(
      static_cast<int64_t>(44100.0 * FREQ_RADIX));
   writeU64LE(data + 12, srFixed);

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::LittleEndian, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_TRUE(result->fields.sampleRate.has_value());
   EXPECT_NEAR(result->fields.sampleRate.value(), 44100.0, FREQ_TOL);
}

// ============================================================================
// Decode — Error Cases
// ============================================================================

TEST(ContextPacketTest, Decode_RejectsDataPacket)
{
   uint8_t data[12];
   writeU32BE(data, 0x10000003);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x00000000);

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);
   EXPECT_FALSE(result.has_value());
}

TEST(ContextPacketTest, Decode_NullData)
{
   size_t consumed = 0;
   auto result = ContextPacket::decode(
      nullptr, 100, ByteOrder::BigEndian, consumed);
   EXPECT_FALSE(result.has_value());
}

TEST(ContextPacketTest, Decode_TruncatedCIF0Field)
{
   // CIF0 says sample rate present (2 words) but packet too small
   uint8_t data[12];
   writeU32BE(data, 0x40000003);  // only 3 words total
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x00200000); // SR needs 2 more words

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);
   EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Encode
// ============================================================================

TEST(ContextPacketTest, Encode_SampleRateOnly_BigEndian)
{
   ContextFields fields;
   fields.sampleRate = 1000000.0;

   auto packet = ContextPacket::encode(
      0x12345678, fields, 0, ByteOrder::BigEndian);

   ASSERT_FALSE(packet.empty());
   EXPECT_EQ(packet.size(), 20u);

   uint32_t word0 = readU32BE(packet.data());
   EXPECT_EQ((word0 >> 28) & 0xFu, 4u);
   EXPECT_EQ(word0 & 0xFFFFu, 5u);

   uint32_t cif0 = readU32BE(packet.data() + 8);
   EXPECT_NE(cif0 & 0x00200000u, 0u);
}

TEST(ContextPacketTest, Encode_MultipleFields)
{
   ContextFields fields;
   fields.bandwidth   = 20000000.0;
   fields.rfFrequency = 2400000000.0;
   fields.sampleRate  = 40000000.0;

   auto packet = ContextPacket::encode(
      0x00000001, fields, 0, ByteOrder::BigEndian);

   ASSERT_FALSE(packet.empty());
   // 1 + 1 + 1(CIF0) + 2(BW) + 2(RF) + 2(SR) = 9 words
   EXPECT_EQ(packet.size(), 36u);

   uint32_t cif0 = readU32BE(packet.data() + 8);
   EXPECT_NE(cif0 & 0x20000000u, 0u);  // Bandwidth
   EXPECT_NE(cif0 & 0x08000000u, 0u);  // RF Frequency
   EXPECT_NE(cif0 & 0x00200000u, 0u);  // Sample Rate
}

TEST(ContextPacketTest, Encode_WithTimestamps)
{
   ContextFields fields;
   fields.sampleRate = 1000.0;

   auto packet = ContextPacket::encode(
      0x00000001, fields, 5, ByteOrder::BigEndian,
      TSI::UTC, TSF::RealTime, 1000, 500);

   ASSERT_FALSE(packet.empty());
   // 1(hdr) + 1(sid) + 1(intTS) + 2(fracTS) + 1(CIF0) + 2(SR) = 8 words
   EXPECT_EQ(packet.size(), 32u);

   uint32_t word0 = readU32BE(packet.data());
   EXPECT_EQ((word0 >> 22) & 0x3u, 1u);   // TSI = UTC
   EXPECT_EQ((word0 >> 20) & 0x3u, 2u);   // TSF = RealTime
   EXPECT_EQ((word0 >> 16) & 0xFu, 5u);   // Packet count
}

// ============================================================================
// Round-Trip
// ============================================================================

TEST(ContextPacketTest, RoundTrip_AllSupportedFields_BigEndian)
{
   ContextFields original;
   original.changeIndicator  = true;
   original.referencePointId = 0xDEADC0DE;
   original.bandwidth        = 10000000.0;
   original.ifRefFrequency   = 70000000.0;
   original.rfFrequency      = 915000000.0;
   original.rfFreqOffset     = 100000.0;
   original.ifBandOffset     = -5000.0;
   original.referenceLevel   = -30.0;
   original.gain             = 15.5;
   original.overRangeCount   = 100;
   original.sampleRate       = 20000000.0;

   auto encoded = ContextPacket::encode(
      0xFEEDFACE, original, 12, ByteOrder::BigEndian,
      TSI::GPS, TSF::SampleCount, 42, 12345);
   ASSERT_FALSE(encoded.empty());

   size_t consumed = 0;
   auto decoded = ContextPacket::decode(
      encoded.data(), encoded.size(), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(decoded.has_value());
   EXPECT_EQ(consumed, encoded.size());

   const auto& f = decoded->fields;
   EXPECT_TRUE(f.changeIndicator);
   ASSERT_TRUE(f.referencePointId.has_value());
   EXPECT_EQ(f.referencePointId.value(), 0xDEADC0DEu);
   ASSERT_TRUE(f.bandwidth.has_value());
   EXPECT_NEAR(f.bandwidth.value(), 10000000.0, FREQ_TOL);
   ASSERT_TRUE(f.ifRefFrequency.has_value());
   EXPECT_NEAR(f.ifRefFrequency.value(), 70000000.0, FREQ_TOL);
   ASSERT_TRUE(f.rfFrequency.has_value());
   EXPECT_NEAR(f.rfFrequency.value(), 915000000.0, FREQ_TOL);
   ASSERT_TRUE(f.rfFreqOffset.has_value());
   EXPECT_NEAR(f.rfFreqOffset.value(), 100000.0, FREQ_TOL);
   ASSERT_TRUE(f.ifBandOffset.has_value());
   EXPECT_NEAR(f.ifBandOffset.value(), -5000.0, FREQ_TOL);
   ASSERT_TRUE(f.referenceLevel.has_value());
   EXPECT_NEAR(f.referenceLevel.value(), -30.0, GAIN_TOL);
   ASSERT_TRUE(f.gain.has_value());
   EXPECT_NEAR(f.gain.value(), 15.5, GAIN_TOL);
   ASSERT_TRUE(f.overRangeCount.has_value());
   EXPECT_EQ(f.overRangeCount.value(), 100u);
   ASSERT_TRUE(f.sampleRate.has_value());
   EXPECT_NEAR(f.sampleRate.value(), 20000000.0, FREQ_TOL);

   EXPECT_EQ(decoded->header.tsiType, TSI::GPS);
   EXPECT_EQ(decoded->header.tsfType, TSF::SampleCount);
   ASSERT_TRUE(decoded->header.integerTimestamp.has_value());
   EXPECT_EQ(decoded->header.integerTimestamp.value(), 42u);
   ASSERT_TRUE(decoded->header.fractionalTimestamp.has_value());
   EXPECT_EQ(decoded->header.fractionalTimestamp.value(), 12345u);
}

TEST(ContextPacketTest, RoundTrip_AllSupportedFields_LittleEndian)
{
   ContextFields original;
   original.bandwidth   = 5000000.0;
   original.rfFrequency = 2400000000.0;
   original.gain        = -10.0;
   original.sampleRate  = 10000000.0;

   auto encoded = ContextPacket::encode(
      0x00000001, original, 0, ByteOrder::LittleEndian);

   size_t consumed = 0;
   auto decoded = ContextPacket::decode(
      encoded.data(), encoded.size(), ByteOrder::LittleEndian, consumed);

   ASSERT_TRUE(decoded.has_value());
   EXPECT_NEAR(decoded->fields.bandwidth.value(),   5000000.0,    FREQ_TOL);
   EXPECT_NEAR(decoded->fields.rfFrequency.value(), 2400000000.0, FREQ_TOL);
   EXPECT_NEAR(decoded->fields.gain.value(),        -10.0,        GAIN_TOL);
   EXPECT_NEAR(decoded->fields.sampleRate.value(),  10000000.0,   FREQ_TOL);
}

TEST(ContextPacketTest, RoundTrip_FieldOrdering)
{
   ContextFields original;
   original.sampleRate = 1000.0;    // bit 21 (lower)
   original.bandwidth  = 500.0;     // bit 29 (higher)

   auto encoded = ContextPacket::encode(
      0x00000001, original, 0, ByteOrder::BigEndian);

   size_t consumed = 0;
   auto decoded = ContextPacket::decode(
      encoded.data(), encoded.size(), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(decoded.has_value());
   EXPECT_NEAR(decoded->fields.bandwidth.value(),  500.0,  FREQ_TOL);
   EXPECT_NEAR(decoded->fields.sampleRate.value(), 1000.0, FREQ_TOL);
}

// ============================================================================
// Fixed-Point Precision
// ============================================================================

TEST(ContextPacketTest, FixedPointPrecision_Frequency)
{
   std::vector<double> freqs = {
      0.0, 1.0, 100.0, 44100.0, 1000000.0, 2400000000.0, 6000000000.0
   };

   for (double freq : freqs)
   {
      ContextFields fields;
      fields.sampleRate = freq;

      auto encoded = ContextPacket::encode(
         0x1, fields, 0, ByteOrder::BigEndian);
      size_t consumed = 0;
      auto decoded = ContextPacket::decode(
         encoded.data(), encoded.size(), ByteOrder::BigEndian, consumed);

      ASSERT_TRUE(decoded.has_value()) << "freq=" << freq;
      ASSERT_TRUE(decoded->fields.sampleRate.has_value()) << "freq=" << freq;
      EXPECT_NEAR(decoded->fields.sampleRate.value(), freq, FREQ_TOL)
         << "freq=" << freq;
   }
}

TEST(ContextPacketTest, FixedPointPrecision_Gain)
{
   std::vector<double> gains = {0.0, 1.0, -1.0, 15.5, -30.0, 100.0, -100.0};

   for (double gain : gains)
   {
      ContextFields fields;
      fields.gain = gain;

      auto encoded = ContextPacket::encode(
         0x1, fields, 0, ByteOrder::BigEndian);
      size_t consumed = 0;
      auto decoded = ContextPacket::decode(
         encoded.data(), encoded.size(), ByteOrder::BigEndian, consumed);

      ASSERT_TRUE(decoded.has_value()) << "gain=" << gain;
      ASSERT_TRUE(decoded->fields.gain.has_value()) << "gain=" << gain;
      EXPECT_NEAR(decoded->fields.gain.value(), gain, GAIN_TOL)
         << "gain=" << gain;
   }
}

// ============================================================================
// Skip Unknown Fields
// ============================================================================

TEST(ContextPacketTest, Decode_SkipsUnsupportedFields)
{
   // CIF0 bits 21 (SR) + 20 (Timestamp Adjustment, 2 words, unsupported)
   // CIF0 = 0x00300000
   // Fields: SR(2 words) then TS Adj(2 words)
   // Total: 1 + 1 + 1(CIF0) + 2(SR) + 2(TSAdj) = 7 words
   uint8_t data[28];
   writeU32BE(data, 0x40000007);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x00300000);

   auto srFixed = static_cast<uint64_t>(
      static_cast<int64_t>(48000.0 * FREQ_RADIX));
   writeU64BE(data + 12, srFixed);
   writeU64BE(data + 20, 0x0000000000001234ull);  // TS Adjustment (skipped)

   size_t consumed = 0;
   auto result = ContextPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_TRUE(result->fields.sampleRate.has_value());
   EXPECT_NEAR(result->fields.sampleRate.value(), 48000.0, FREQ_TOL);
}
