/**
 * @file SignalDataPacketUt.cpp
 * @brief Unit tests for Vita49_2::SignalDataPacket.
 */

#include <gtest/gtest.h>
#include "SignalDataPacket.h"
#include "ByteSwap.h"

#include <cmath>

using namespace Vita49_2;

static constexpr float SCALE = 32768.0f;
// Quantization tolerance: 1 LSB in float + epsilon
static constexpr float TOLERANCE = (1.0f / SCALE) + 1e-6f;

// ============================================================================
// Decode — Known Byte Patterns
// ============================================================================

TEST(SignalDataPacketTest, Decode_SingleSample_BigEndian)
{
   // Type 1, size=3, streamId=0x12345678, I=1000, Q=-500
   uint8_t data[12];
   writeU32BE(data, 0x10000003);
   writeU32BE(data + 4, 0x12345678);
   writeI16BE(data + 8, 1000);
   writeI16BE(data + 10, -500);

   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, SCALE, consumed);

   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(consumed, 12u);
   EXPECT_EQ(result->header.packetType, PacketType::IFDataWithStreamId);
   ASSERT_TRUE(result->header.streamId.has_value());
   EXPECT_EQ(result->header.streamId.value(), 0x12345678u);

   ASSERT_EQ(result->samples.size(), 1u);
   EXPECT_NEAR(result->samples[0].real(),  1000.0f / SCALE, 1e-6f);
   EXPECT_NEAR(result->samples[0].imag(), -500.0f  / SCALE, 1e-6f);
}

TEST(SignalDataPacketTest, Decode_MultipleSamples_BigEndian)
{
   uint8_t data[24]; // 2 header + 4 payload = 6 words
   writeU32BE(data, 0x10000006);
   writeU32BE(data + 4, 0x00000001);

   int16_t samples[][2] = {
      {100, 200}, {-300, 400}, {500, -600}, {32767, -32768}
   };
   for (ptrdiff_t i = 0; i < 4; ++i)
   {
      writeI16BE(data + 8 + (i * 4), samples[i][0]);
      writeI16BE(data + 8 + (i * 4) + 2, samples[i][1]);
   }

   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, SCALE, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_EQ(result->samples.size(), 4u);
   for (int i = 0; i < 4; ++i)
   {
      EXPECT_NEAR(result->samples[static_cast<size_t>(i)].real(),
                  static_cast<float>(samples[i][0]) / SCALE, 1e-6f);
      EXPECT_NEAR(result->samples[static_cast<size_t>(i)].imag(),
                  static_cast<float>(samples[i][1]) / SCALE, 1e-6f);
   }
}

TEST(SignalDataPacketTest, Decode_LittleEndian)
{
   uint8_t data[12];
   writeU32LE(data, 0x10000003);
   writeU32LE(data + 4, 0xAABBCCDD);
   writeI16LE(data + 8, 2000);
   writeI16LE(data + 10, -1000);

   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      data, sizeof(data), ByteOrder::LittleEndian, SCALE, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_EQ(result->samples.size(), 1u);
   EXPECT_NEAR(result->samples[0].real(),  2000.0f  / SCALE, 1e-6f);
   EXPECT_NEAR(result->samples[0].imag(), -1000.0f / SCALE, 1e-6f);
}

TEST(SignalDataPacketTest, Decode_WithTrailer_BigEndian)
{
   // T=1 (bit 26): 0x14000004 = type 1, trailer, size=4
   uint8_t data[16];
   writeU32BE(data, 0x14000004);
   writeU32BE(data + 4, 0x00000001);
   writeI16BE(data + 8, 5000);
   writeI16BE(data + 10, -3000);
   writeU32BE(data + 12, 0x00000000);  // Trailer

   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, SCALE, consumed);

   ASSERT_TRUE(result.has_value());
   EXPECT_TRUE(result->header.trailerPresent);
   ASSERT_EQ(result->samples.size(), 1u);
   EXPECT_NEAR(result->samples[0].real(),  5000.0f  / SCALE, 1e-6f);
   EXPECT_NEAR(result->samples[0].imag(), -3000.0f / SCALE, 1e-6f);
}

TEST(SignalDataPacketTest, Decode_CustomScaleFactor)
{
   uint8_t data[12];
   writeU32BE(data, 0x10000003);
   writeU32BE(data + 4, 0x00000001);
   writeI16BE(data + 8, 1000);
   writeI16BE(data + 10, -1000);

   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, 1000.0f, consumed);

   ASSERT_TRUE(result.has_value());
   ASSERT_EQ(result->samples.size(), 1u);
   EXPECT_NEAR(result->samples[0].real(),  1.0f, 1e-6f);
   EXPECT_NEAR(result->samples[0].imag(), -1.0f, 1e-6f);
}

TEST(SignalDataPacketTest, Decode_EmptyPayload)
{
   uint8_t data[8];
   writeU32BE(data, 0x10000002);
   writeU32BE(data + 4, 0x00000001);

   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, SCALE, consumed);

   ASSERT_TRUE(result.has_value());
   EXPECT_TRUE(result->samples.empty());
}

// ============================================================================
// Decode — Error Cases
// ============================================================================

TEST(SignalDataPacketTest, Decode_RejectsContextPacket)
{
   uint8_t data[12];
   writeU32BE(data, 0x40000003);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x00000000);

   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, SCALE, consumed);
   EXPECT_FALSE(result.has_value());
}

TEST(SignalDataPacketTest, Decode_NullData)
{
   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      nullptr, 100, ByteOrder::BigEndian, SCALE, consumed);
   EXPECT_FALSE(result.has_value());
}

TEST(SignalDataPacketTest, Decode_ZeroScaleFactor)
{
   uint8_t data[12];
   writeU32BE(data, 0x10000003);
   writeU32BE(data + 4, 0x00000001);
   writeU32BE(data + 8, 0x03E8FE0C);

   size_t consumed = 0;
   auto result = SignalDataPacket::decode(
      data, sizeof(data), ByteOrder::BigEndian, 0.0f, consumed);
   EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Encode
// ============================================================================

TEST(SignalDataPacketTest, Encode_SingleSample_BigEndian)
{
   IQSamples samples = {{0.5f, -0.25f}};
   auto packet = SignalDataPacket::encode(
      0x12345678, samples, 0, ByteOrder::BigEndian, SCALE);

   ASSERT_FALSE(packet.empty());
   EXPECT_EQ(packet.size(), 12u);

   uint32_t word0 = readU32BE(packet.data());
   EXPECT_EQ((word0 >> 28) & 0xFu, 1u);
   EXPECT_EQ(word0 & 0xFFFFu, 3u);
   EXPECT_EQ(readU32BE(packet.data() + 4), 0x12345678u);

   int16_t iVal = readI16BE(packet.data() + 8);
   int16_t qVal = readI16BE(packet.data() + 10);
   EXPECT_EQ(iVal, static_cast<int16_t>(std::lroundf(0.5f * SCALE)));
   EXPECT_EQ(qVal, static_cast<int16_t>(std::lroundf(-0.25f * SCALE)));
}

TEST(SignalDataPacketTest, Encode_LittleEndian)
{
   IQSamples samples = {{1.0f / SCALE, -1.0f / SCALE}};
   auto packet = SignalDataPacket::encode(
      0x87654321, samples, 3, ByteOrder::LittleEndian, SCALE);

   ASSERT_FALSE(packet.empty());
   uint32_t word0 = readU32LE(packet.data());
   EXPECT_EQ((word0 >> 16) & 0xFu, 3u);

   int16_t iVal = readI16LE(packet.data() + 8);
   int16_t qVal = readI16LE(packet.data() + 10);
   EXPECT_EQ(iVal, 1);
   EXPECT_EQ(qVal, -1);
}

TEST(SignalDataPacketTest, Encode_WithTimestamps)
{
   IQSamples samples = {{0.1f, -0.1f}};
   auto packet = SignalDataPacket::encode(
      0x00000001, samples, 0, ByteOrder::BigEndian, SCALE,
      TSI::UTC, TSF::RealTime, 1000, 500);

   ASSERT_FALSE(packet.empty());
   // 1(hdr) + 1(sid) + 1(intTS) + 2(fracTS) + 1(payload) = 6 words = 24 bytes
   EXPECT_EQ(packet.size(), 24u);

   uint32_t word0 = readU32BE(packet.data());
   EXPECT_EQ(word0 & 0xFFFFu, 6u);
   EXPECT_EQ((word0 >> 22) & 0x3u, 1u);  // TSI = UTC
   EXPECT_EQ((word0 >> 20) & 0x3u, 2u);  // TSF = RealTime
}

TEST(SignalDataPacketTest, Encode_WithTrailer)
{
   IQSamples samples = {{0.5f, 0.5f}};
   auto packet = SignalDataPacket::encode(
      0x00000001, samples, 0, ByteOrder::BigEndian, SCALE,
      TSI::None, TSF::None, 0, 0, true);

   // 2(hdr+sid) + 1(payload) + 1(trailer) = 4 words
   EXPECT_EQ(packet.size(), 16u);
   uint32_t word0 = readU32BE(packet.data());
   EXPECT_TRUE(((word0 >> 26) & 0x1u) != 0);
   EXPECT_EQ(word0 & 0xFFFFu, 4u);
}

TEST(SignalDataPacketTest, Encode_ClampsSamples)
{
   IQSamples samples = {{2.0f, -2.0f}};
   auto packet = SignalDataPacket::encode(
      0x00000001, samples, 0, ByteOrder::BigEndian, SCALE);

   ASSERT_FALSE(packet.empty());
   int16_t iVal = readI16BE(packet.data() + 8);
   int16_t qVal = readI16BE(packet.data() + 10);
   EXPECT_EQ(iVal, 32767);
   EXPECT_EQ(qVal, -32768);
}

TEST(SignalDataPacketTest, Encode_EmptySamples)
{
   IQSamples samples;
   auto packet = SignalDataPacket::encode(
      0x00000001, samples, 0, ByteOrder::BigEndian, SCALE);

   ASSERT_FALSE(packet.empty());
   EXPECT_EQ(packet.size(), 8u);
}

// ============================================================================
// Round-Trip
// ============================================================================

TEST(SignalDataPacketTest, RoundTrip_BigEndian)
{
   IQSamples original = {
      {0.5f, -0.5f}, {0.25f, -0.25f}, {0.0f, 0.0f}, {0.75f, -0.75f}
   };

   auto encoded = SignalDataPacket::encode(
      0xDEADCAFE, original, 7, ByteOrder::BigEndian, SCALE);
   ASSERT_FALSE(encoded.empty());

   size_t consumed = 0;
   auto decoded = SignalDataPacket::decode(
      encoded.data(), encoded.size(), ByteOrder::BigEndian, SCALE, consumed);

   ASSERT_TRUE(decoded.has_value());
   EXPECT_EQ(consumed, encoded.size());
   ASSERT_EQ(decoded->samples.size(), original.size());

   for (size_t i = 0; i < original.size(); ++i)
   {
      EXPECT_NEAR(decoded->samples[i].real(), original[i].real(), TOLERANCE);
      EXPECT_NEAR(decoded->samples[i].imag(), original[i].imag(), TOLERANCE);
   }

   ASSERT_TRUE(decoded->header.streamId.has_value());
   EXPECT_EQ(decoded->header.streamId.value(), 0xDEADCAFEu);
   EXPECT_EQ(decoded->header.packetCount, 7);
}

TEST(SignalDataPacketTest, RoundTrip_LittleEndian)
{
   IQSamples original = {{0.1f, -0.9f}, {-0.5f, 0.3f}};

   auto encoded = SignalDataPacket::encode(
      0x11223344, original, 15, ByteOrder::LittleEndian, SCALE);
   ASSERT_FALSE(encoded.empty());

   size_t consumed = 0;
   auto decoded = SignalDataPacket::decode(
      encoded.data(), encoded.size(), ByteOrder::LittleEndian, SCALE, consumed);

   ASSERT_TRUE(decoded.has_value());
   ASSERT_EQ(decoded->samples.size(), original.size());

   for (size_t i = 0; i < original.size(); ++i)
   {
      EXPECT_NEAR(decoded->samples[i].real(), original[i].real(), TOLERANCE);
      EXPECT_NEAR(decoded->samples[i].imag(), original[i].imag(), TOLERANCE);
   }
}

TEST(SignalDataPacketTest, RoundTrip_WithTimestamps)
{
   IQSamples original = {{0.5f, -0.5f}};

   auto encoded = SignalDataPacket::encode(
      0x00000001, original, 0, ByteOrder::BigEndian, SCALE,
      TSI::GPS, TSF::SampleCount, 42, 12345);

   size_t consumed = 0;
   auto decoded = SignalDataPacket::decode(
      encoded.data(), encoded.size(), ByteOrder::BigEndian, SCALE, consumed);

   ASSERT_TRUE(decoded.has_value());
   EXPECT_EQ(decoded->header.tsiType, TSI::GPS);
   EXPECT_EQ(decoded->header.tsfType, TSF::SampleCount);
   ASSERT_TRUE(decoded->header.integerTimestamp.has_value());
   EXPECT_EQ(decoded->header.integerTimestamp.value(), 42u);
   ASSERT_TRUE(decoded->header.fractionalTimestamp.has_value());
   EXPECT_EQ(decoded->header.fractionalTimestamp.value(), 12345u);
}

// ============================================================================
// Max Samples
// ============================================================================

TEST(SignalDataPacketTest, MaxSamplesPerPacket_Minimal)
{
   EXPECT_EQ(SignalDataPacket::maxSamplesPerPacket(), 65533u);
}

TEST(SignalDataPacketTest, MaxSamplesPerPacket_WithOverhead)
{
   size_t max = SignalDataPacket::maxSamplesPerPacket(
      TSI::UTC, TSF::RealTime, true, true);
   // 65535 - 7 (hdr+sid+classId+intTS+fracTS) - 1 (trailer) = 65527
   EXPECT_EQ(max, 65527u);
}
