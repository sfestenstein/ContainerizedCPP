/**
 * @file Vita49CodecUt.cpp
 * @brief Unit tests for Vita49_2::Vita49Codec (high-level stream codec).
 */

#include <gtest/gtest.h>
#include "Vita49Codec.h"

#include <cmath>

using namespace Vita49_2;

static constexpr float SCALE     = DEFAULT_SCALE_FACTOR;
static constexpr float TOLERANCE = (1.0f / SCALE) + 1e-6f;

// ============================================================================
// Construction & Configuration
// ============================================================================

TEST(Vita49CodecTest, DefaultConstruction)
{
   Vita49Codec codec;
   EXPECT_EQ(codec.byteOrder(), ByteOrder::BigEndian);
   EXPECT_FLOAT_EQ(codec.scaleFactor(), DEFAULT_SCALE_FACTOR);
}

TEST(Vita49CodecTest, CustomConstruction)
{
   Vita49Codec codec(ByteOrder::LittleEndian, 1000.0f);
   EXPECT_EQ(codec.byteOrder(), ByteOrder::LittleEndian);
   EXPECT_FLOAT_EQ(codec.scaleFactor(), 1000.0f);
}

TEST(Vita49CodecTest, SettersAndGetters)
{
   Vita49Codec codec;
   codec.setByteOrder(ByteOrder::LittleEndian);
   codec.setScaleFactor(2048.0f);
   EXPECT_EQ(codec.byteOrder(), ByteOrder::LittleEndian);
   EXPECT_FLOAT_EQ(codec.scaleFactor(), 2048.0f);
}

// ============================================================================
// parsePacket
// ============================================================================

TEST(Vita49CodecTest, ParsePacket_SignalData)
{
   Vita49Codec codec;
   IQSamples samples = {{0.5f, -0.25f}};
   auto encoded = codec.encodeSignalData(0x1, samples);

   size_t consumed = 0;
   auto parsed = codec.parsePacket(
      encoded.data(), encoded.size(), consumed);

   ASSERT_TRUE(parsed.has_value());
   EXPECT_EQ(parsed->type, ParsedPacket::Type::SignalData);
   ASSERT_EQ(parsed->samples.size(), 1u);
   EXPECT_NEAR(parsed->samples[0].real(),  0.5f,  TOLERANCE);
   EXPECT_NEAR(parsed->samples[0].imag(), -0.25f, TOLERANCE);
}

TEST(Vita49CodecTest, ParsePacket_Context)
{
   Vita49Codec codec;
   ContextFields fields;
   fields.sampleRate = 1000000.0;
   auto encoded = codec.encodeContext(0x1, fields);

   size_t consumed = 0;
   auto parsed = codec.parsePacket(
      encoded.data(), encoded.size(), consumed);

   ASSERT_TRUE(parsed.has_value());
   EXPECT_EQ(parsed->type, ParsedPacket::Type::Context);
   ASSERT_TRUE(parsed->contextFields.sampleRate.has_value());
   EXPECT_NEAR(parsed->contextFields.sampleRate.value(), 1000000.0, 1e-6);
}

TEST(Vita49CodecTest, ParsePacket_NullData)
{
   Vita49Codec codec;
   size_t consumed = 0;
   auto parsed = codec.parsePacket(nullptr, 100, consumed);
   EXPECT_FALSE(parsed.has_value());
}

TEST(Vita49CodecTest, ParsePacket_EmptyBuffer)
{
   Vita49Codec codec;
   uint8_t data[1] = {0};
   size_t consumed = 0;
   auto parsed = codec.parsePacket(data, 0, consumed);
   EXPECT_FALSE(parsed.has_value());
}

// ============================================================================
// parseStream — Single Packet
// ============================================================================

TEST(Vita49CodecTest, ParseStream_SinglePacket)
{
   Vita49Codec codec;
   IQSamples samples = {{0.1f, -0.2f}, {0.3f, -0.4f}};
   auto encoded = codec.encodeSignalData(0x1, samples);

   auto parsed = codec.parseStream(encoded.data(), encoded.size());

   ASSERT_EQ(parsed.size(), 1u);
   EXPECT_EQ(parsed[0].type, ParsedPacket::Type::SignalData);
   ASSERT_EQ(parsed[0].samples.size(), 2u);
}

// ============================================================================
// parseStream — Multiple Packets
// ============================================================================

TEST(Vita49CodecTest, ParseStream_MultipleSignalDataPackets)
{
   Vita49Codec codec;
   IQSamples s1 = {{0.5f, -0.5f}};
   IQSamples s2 = {{0.25f, -0.25f}};

   auto pkt1 = codec.encodeSignalData(0x1, s1, 0);
   auto pkt2 = codec.encodeSignalData(0x1, s2, 1);

   std::vector<uint8_t> stream;
   stream.insert(stream.end(), pkt1.begin(), pkt1.end());
   stream.insert(stream.end(), pkt2.begin(), pkt2.end());

   auto parsed = codec.parseStream(stream.data(), stream.size());

   ASSERT_EQ(parsed.size(), 2u);
   EXPECT_EQ(parsed[0].type, ParsedPacket::Type::SignalData);
   EXPECT_EQ(parsed[1].type, ParsedPacket::Type::SignalData);
   EXPECT_NEAR(parsed[0].samples[0].real(), 0.5f,  TOLERANCE);
   EXPECT_NEAR(parsed[1].samples[0].real(), 0.25f, TOLERANCE);
}

TEST(Vita49CodecTest, ParseStream_MixedPacketTypes)
{
   Vita49Codec codec;

   ContextFields fields;
   fields.sampleRate = 48000.0;
   auto ctxPkt = codec.encodeContext(0x1, fields);

   IQSamples samples = {{0.5f, -0.5f}, {0.1f, 0.2f}};
   auto dataPkt = codec.encodeSignalData(0x1, samples);

   std::vector<uint8_t> stream;
   stream.insert(stream.end(), ctxPkt.begin(), ctxPkt.end());
   stream.insert(stream.end(), dataPkt.begin(), dataPkt.end());

   auto parsed = codec.parseStream(stream.data(), stream.size());

   ASSERT_EQ(parsed.size(), 2u);
   EXPECT_EQ(parsed[0].type, ParsedPacket::Type::Context);
   EXPECT_EQ(parsed[1].type, ParsedPacket::Type::SignalData);
   ASSERT_TRUE(parsed[0].contextFields.sampleRate.has_value());
   EXPECT_NEAR(parsed[0].contextFields.sampleRate.value(), 48000.0, 1e-6);
   ASSERT_EQ(parsed[1].samples.size(), 2u);
}

TEST(Vita49CodecTest, ParseStream_EmptyBuffer)
{
   Vita49Codec codec;
   auto parsed = codec.parseStream(nullptr, 0);
   EXPECT_TRUE(parsed.empty());
}

TEST(Vita49CodecTest, ParseStream_StopsOnTrailingBytes)
{
   Vita49Codec codec;
   IQSamples samples = {{0.1f, 0.2f}};
   auto encoded = codec.encodeSignalData(0x1, samples);

   // Append 2 trailing bytes
   encoded.push_back(0xAA);
   encoded.push_back(0xBB);

   auto parsed = codec.parseStream(encoded.data(), encoded.size());
   ASSERT_EQ(parsed.size(), 1u);
}

// ============================================================================
// encodeSignalData
// ============================================================================

TEST(Vita49CodecTest, EncodeSignalData_SinglePacket)
{
   Vita49Codec codec;
   IQSamples samples = {{0.1f, 0.2f}, {0.3f, 0.4f}};
   auto encoded = codec.encodeSignalData(0x42, samples);

   ASSERT_FALSE(encoded.empty());
   auto parsed = codec.parseStream(encoded.data(), encoded.size());
   ASSERT_EQ(parsed.size(), 1u);
   ASSERT_EQ(parsed[0].samples.size(), 2u);
}

TEST(Vita49CodecTest, EncodeSignalData_EmptySamples)
{
   Vita49Codec codec;
   IQSamples samples;
   auto encoded = codec.encodeSignalData(0x1, samples);

   ASSERT_FALSE(encoded.empty());
   auto parsed = codec.parseStream(encoded.data(), encoded.size());
   ASSERT_EQ(parsed.size(), 1u);
   EXPECT_TRUE(parsed[0].samples.empty());
}

// ============================================================================
// encodeContext
// ============================================================================

TEST(Vita49CodecTest, EncodeContext_RoundTrip)
{
   Vita49Codec codec;
   ContextFields fields;
   fields.sampleRate  = 2000000.0;
   fields.rfFrequency = 1575420000.0;
   fields.bandwidth   = 2046000.0;

   auto encoded = codec.encodeContext(0x42, fields, 3);

   auto parsed = codec.parseStream(encoded.data(), encoded.size());
   ASSERT_EQ(parsed.size(), 1u);
   EXPECT_EQ(parsed[0].type, ParsedPacket::Type::Context);
   EXPECT_EQ(parsed[0].header.packetCount, 3);
   ASSERT_TRUE(parsed[0].header.streamId.has_value());
   EXPECT_EQ(parsed[0].header.streamId.value(), 0x42u);
}

// ============================================================================
// Byte Order Round-Trips
// ============================================================================

TEST(Vita49CodecTest, ByteOrder_BigEndianRoundTrip)
{
   Vita49Codec codec(ByteOrder::BigEndian);
   IQSamples samples = {{0.7f, -0.3f}};
   auto encoded = codec.encodeSignalData(0x1, samples);
   auto parsed  = codec.parseStream(encoded.data(), encoded.size());

   ASSERT_EQ(parsed.size(), 1u);
   EXPECT_NEAR(parsed[0].samples[0].real(),  0.7f, TOLERANCE);
   EXPECT_NEAR(parsed[0].samples[0].imag(), -0.3f, TOLERANCE);
}

TEST(Vita49CodecTest, ByteOrder_LittleEndianRoundTrip)
{
   Vita49Codec codec(ByteOrder::LittleEndian);
   IQSamples samples = {{0.7f, -0.3f}};
   auto encoded = codec.encodeSignalData(0x1, samples);
   auto parsed  = codec.parseStream(encoded.data(), encoded.size());

   ASSERT_EQ(parsed.size(), 1u);
   EXPECT_NEAR(parsed[0].samples[0].real(),  0.7f, TOLERANCE);
   EXPECT_NEAR(parsed[0].samples[0].imag(), -0.3f, TOLERANCE);
}

TEST(Vita49CodecTest, ByteOrder_MismatchProducesBadData)
{
   Vita49Codec beCodec(ByteOrder::BigEndian);
   Vita49Codec leCodec(ByteOrder::LittleEndian);

   IQSamples samples = {{0.5f, -0.5f}};
   auto encoded = beCodec.encodeSignalData(0x1, samples);

   // Mismatched decode — should fail or produce wrong values
   auto parsed = leCodec.parseStream(encoded.data(), encoded.size());
   if (!parsed.empty() && !parsed[0].samples.empty())
   {
      bool valuesMatch =
         std::abs(parsed[0].samples[0].real() - 0.5f) < TOLERANCE &&
         std::abs(parsed[0].samples[0].imag() - (-0.5f)) < TOLERANCE;
      EXPECT_FALSE(valuesMatch);
   }
}

// ============================================================================
// Scale Factor
// ============================================================================

TEST(Vita49CodecTest, ScaleFactor_Custom)
{
   Vita49Codec codec(ByteOrder::BigEndian, 1000.0f);
   IQSamples samples = {{0.5f, -0.5f}};
   auto encoded = codec.encodeSignalData(0x1, samples);
   auto parsed  = codec.parseStream(encoded.data(), encoded.size());

   ASSERT_EQ(parsed.size(), 1u);
   EXPECT_NEAR(parsed[0].samples[0].real(),  0.5f, 0.001f);
   EXPECT_NEAR(parsed[0].samples[0].imag(), -0.5f, 0.001f);
}

TEST(Vita49CodecTest, ScaleFactor_LargeValuesClamped)
{
   Vita49Codec codec(ByteOrder::BigEndian, 1.0f);
   IQSamples samples = {{50000.0f, -50000.0f}};
   auto encoded = codec.encodeSignalData(0x1, samples);
   auto parsed  = codec.parseStream(encoded.data(), encoded.size());

   ASSERT_EQ(parsed.size(), 1u);
   EXPECT_FLOAT_EQ(parsed[0].samples[0].real(),  32767.0f);
   EXPECT_FLOAT_EQ(parsed[0].samples[0].imag(), -32768.0f);
}

// ============================================================================
// Complex Stream Scenarios
// ============================================================================

TEST(Vita49CodecTest, ComplexStream_ContextThenMultipleData)
{
   Vita49Codec codec;

   ContextFields ctx;
   ctx.sampleRate  = 10000000.0;
   ctx.rfFrequency = 915000000.0;
   ctx.bandwidth   = 5000000.0;
   ctx.gain        = 20.0;

   IQSamples data1(100);
   IQSamples data2(50);
   for (size_t i = 0; i < data1.size(); ++i)
   {
      float phase = static_cast<float>(i) * 0.1f;
      data1[i] = {std::cos(phase) * 0.8f, std::sin(phase) * 0.8f};
   }
   for (size_t i = 0; i < data2.size(); ++i)
   {
      float phase = static_cast<float>(i) * 0.2f;
      data2[i] = {std::cos(phase) * 0.5f, std::sin(phase) * 0.5f};
   }

   auto ctxPkt  = codec.encodeContext(0x1, ctx);
   auto dataPkt1 = codec.encodeSignalData(0x1, data1, 0);
   auto dataPkt2 = codec.encodeSignalData(0x1, data2, 1);

   std::vector<uint8_t> stream;
   stream.insert(stream.end(), ctxPkt.begin(),   ctxPkt.end());
   stream.insert(stream.end(), dataPkt1.begin(), dataPkt1.end());
   stream.insert(stream.end(), dataPkt2.begin(), dataPkt2.end());

   auto parsed = codec.parseStream(stream.data(), stream.size());

   ASSERT_EQ(parsed.size(), 3u);
   EXPECT_EQ(parsed[0].type, ParsedPacket::Type::Context);
   EXPECT_EQ(parsed[1].type, ParsedPacket::Type::SignalData);
   EXPECT_EQ(parsed[2].type, ParsedPacket::Type::SignalData);

   EXPECT_NEAR(parsed[0].contextFields.sampleRate.value(), 10000000.0, 1e-6);
   EXPECT_EQ(parsed[1].samples.size(), 100u);
   EXPECT_EQ(parsed[2].samples.size(), 50u);

   // Verify sample data integrity
   for (size_t i = 0; i < data1.size(); ++i)
   {
      EXPECT_NEAR(parsed[1].samples[i].real(), data1[i].real(), TOLERANCE);
      EXPECT_NEAR(parsed[1].samples[i].imag(), data1[i].imag(), TOLERANCE);
   }
}

TEST(Vita49CodecTest, ParseStream_LargeNumberOfSamples)
{
   Vita49Codec codec;

   IQSamples samples(1000);
   for (size_t i = 0; i < samples.size(); ++i)
   {
      float t = static_cast<float>(i) / 1000.0f;
      samples[i] = {
         std::sin(t * 6.28f) * 0.9f,
         std::cos(t * 6.28f) * 0.9f
      };
   }

   auto encoded = codec.encodeSignalData(0x42, samples);
   auto parsed  = codec.parseStream(encoded.data(), encoded.size());

   ASSERT_EQ(parsed.size(), 1u);
   ASSERT_EQ(parsed[0].samples.size(), 1000u);

   for (size_t i = 0; i < samples.size(); ++i)
   {
      EXPECT_NEAR(parsed[0].samples[i].real(), samples[i].real(), TOLERANCE);
      EXPECT_NEAR(parsed[0].samples[i].imag(), samples[i].imag(), TOLERANCE);
   }
}

TEST(Vita49CodecTest, ParseStream_MultipleContextPackets)
{
   Vita49Codec codec;

   ContextFields f1;
   f1.sampleRate = 1000000.0;
   f1.rfFrequency = 900000000.0;

   ContextFields f2;
   f2.sampleRate = 2000000.0;
   f2.gain = -5.0;
   f2.changeIndicator = true;

   auto pkt1 = codec.encodeContext(0x1, f1, 0);
   auto pkt2 = codec.encodeContext(0x1, f2, 1);

   std::vector<uint8_t> stream;
   stream.insert(stream.end(), pkt1.begin(), pkt1.end());
   stream.insert(stream.end(), pkt2.begin(), pkt2.end());

   auto parsed = codec.parseStream(stream.data(), stream.size());
   ASSERT_EQ(parsed.size(), 2u);
   EXPECT_EQ(parsed[0].type, ParsedPacket::Type::Context);
   EXPECT_EQ(parsed[1].type, ParsedPacket::Type::Context);
   EXPECT_FALSE(parsed[0].contextFields.changeIndicator);
   EXPECT_TRUE(parsed[1].contextFields.changeIndicator);
}
