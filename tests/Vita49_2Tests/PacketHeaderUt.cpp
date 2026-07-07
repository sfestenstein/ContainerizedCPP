/**
 * @file PacketHeaderUt.cpp
 * @brief Unit tests for Vita49_2::PacketHeaderCodec.
 */

#include <gtest/gtest.h>
#include "PacketHeader.h"
#include "ByteSwap.h"

using namespace Vita49_2;

// ============================================================================
// Parsing — Word 0 Fields
// ============================================================================

TEST(PacketHeaderTest, ParseMinimalHeader_BigEndian)
{
   // IF Data without Stream ID, no options, packet size = 1
   uint8_t data[4];
   writeU32BE(data, 0x00000001);

   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_TRUE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));

   EXPECT_EQ(header.packetType, PacketType::IFDataWithoutStreamId);
   EXPECT_FALSE(header.classIdPresent);
   EXPECT_FALSE(header.trailerPresent);
   EXPECT_EQ(header.tsiType, TSI::None);
   EXPECT_EQ(header.tsfType, TSF::None);
   EXPECT_EQ(header.packetCount, 0);
   EXPECT_EQ(header.packetSize, 1);
   EXPECT_FALSE(header.streamId.has_value());
   EXPECT_EQ(headerBytes, 4u);
}

TEST(PacketHeaderTest, ParseWithStreamId_BigEndian)
{
   // Type 1 (IF Data with Stream ID), packet size = 2
   uint8_t data[8];
   writeU32BE(data, 0x10000002);        // Type 1, size 2
   writeU32BE(data + 4, 0x12345678);   // Stream ID

   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_TRUE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));

   EXPECT_EQ(header.packetType, PacketType::IFDataWithStreamId);
   ASSERT_TRUE(header.streamId.has_value());
   EXPECT_EQ(header.streamId.value(), 0x12345678u);
   EXPECT_EQ(header.packetSize, 2);
   EXPECT_EQ(headerBytes, 8u);
}

TEST(PacketHeaderTest, ParseWithClassId_BigEndian)
{
   // Type 1, C=1 → 0001 1 000 ... → 0x18000004
   uint8_t data[16];
   writeU32BE(data, 0x18000004);
   writeU32BE(data + 4, 0xAABBCCDD);      // Stream ID
   writeU32BE(data + 8, 0x00123456);      // OUI
   writeU32BE(data + 12, 0x00780099);     // ICC=0x0078, PCC=0x0099

   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_TRUE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));

   EXPECT_TRUE(header.classIdPresent);
   ASSERT_TRUE(header.classIdOUI.has_value());
   EXPECT_EQ(header.classIdOUI.value(), 0x123456u);
   ASSERT_TRUE(header.informationClassCode.has_value());
   EXPECT_EQ(header.informationClassCode.value(), 0x0078u);
   ASSERT_TRUE(header.packetClassCode.has_value());
   EXPECT_EQ(header.packetClassCode.value(), 0x0099u);
   EXPECT_EQ(headerBytes, 16u);
}

TEST(PacketHeaderTest, ParseWithTimestamps_BigEndian)
{
   // Type 1, TSI=UTC(01 bits 23-22), TSF=RealTime(10 bits 21-20)
   // 0001 0 000 01 10 0000 ... = 0x10600005
   uint8_t data[20];
   writeU32BE(data, 0x10600005);
   writeU32BE(data + 4, 0x11111111);
   writeU32BE(data + 8, 0xDEADBEEF);
   writeU64BE(data + 12, 0x0123456789ABCDEFull);

   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_TRUE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));

   EXPECT_EQ(header.tsiType, TSI::UTC);
   EXPECT_EQ(header.tsfType, TSF::RealTime);
   ASSERT_TRUE(header.integerTimestamp.has_value());
   EXPECT_EQ(header.integerTimestamp.value(), 0xDEADBEEFu);
   ASSERT_TRUE(header.fractionalTimestamp.has_value());
   EXPECT_EQ(header.fractionalTimestamp.value(), 0x0123456789ABCDEFull);
   EXPECT_EQ(headerBytes, 20u);
}

TEST(PacketHeaderTest, ParseTrailerBit_BigEndian)
{
   // Type 1, Trailer (bit 26): 0001 0 1 00 ... = 0x14000003
   uint8_t data[12];
   writeU32BE(data, 0x14000003);
   writeU32BE(data + 4, 0x12345678);
   writeU32BE(data + 8, 0x00000000);  // Trailer word

   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_TRUE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));

   EXPECT_TRUE(header.trailerPresent);
}

TEST(PacketHeaderTest, ParseContextPacket_BigEndian)
{
   // Context (type 4): 0100 0 000 ... = 0x40000003
   uint8_t data[12];
   writeU32BE(data, 0x40000003);
   writeU32BE(data + 4, 0xABCDEF01);
   writeU32BE(data + 8, 0x00000000);  // CIF0

   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_TRUE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));

   EXPECT_EQ(header.packetType, PacketType::IFContext);
   ASSERT_TRUE(header.streamId.has_value());
   EXPECT_EQ(header.streamId.value(), 0xABCDEF01u);
   EXPECT_FALSE(header.trailerPresent);
}

TEST(PacketHeaderTest, ParseLittleEndian)
{
   uint8_t data[8];
   writeU32LE(data, 0x10000002);
   writeU32LE(data + 4, 0x12345678);

   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_TRUE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::LittleEndian, header, headerBytes));

   EXPECT_EQ(header.packetType, PacketType::IFDataWithStreamId);
   ASSERT_TRUE(header.streamId.has_value());
   EXPECT_EQ(header.streamId.value(), 0x12345678u);
}

// ============================================================================
// Parsing — Error Cases
// ============================================================================

TEST(PacketHeaderTest, ParseFailsOnTruncatedBuffer)
{
   uint8_t data[2] = {0x10, 0x00};
   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_FALSE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));
}

TEST(PacketHeaderTest, ParseFailsOnNullData)
{
   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_FALSE(PacketHeaderCodec::parse(
      nullptr, 100, ByteOrder::BigEndian, header, headerBytes));
}

TEST(PacketHeaderTest, ParseFailsOnZeroPacketSize)
{
   uint8_t data[4];
   writeU32BE(data, 0x10000000);  // Size = 0
   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_FALSE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));
}

TEST(PacketHeaderTest, ParseFailsOnInsufficientDataForPacket)
{
   uint8_t data[4];
   writeU32BE(data, 0x1000000A);  // Size = 10 words, but only 4 bytes
   PacketHeader header;
   size_t headerBytes = 0;
   EXPECT_FALSE(PacketHeaderCodec::parse(
      data, sizeof(data), ByteOrder::BigEndian, header, headerBytes));
}

// ============================================================================
// Serialization
// ============================================================================

TEST(PacketHeaderTest, SerializeMinimalHeader_BigEndian)
{
   PacketHeader header;
   header.packetType = PacketType::IFDataWithoutStreamId;
   header.packetSize = 1;

   std::vector<uint8_t> out;
   PacketHeaderCodec::serialize(header, ByteOrder::BigEndian, out);

   ASSERT_EQ(out.size(), 4u);
   EXPECT_EQ(readU32BE(out.data()), 0x00000001u);
}

TEST(PacketHeaderTest, SerializeWithStreamId_BigEndian)
{
   PacketHeader header;
   header.packetType = PacketType::IFDataWithStreamId;
   header.streamId   = 0xDEADCAFE;
   header.packetSize = 2;

   std::vector<uint8_t> out;
   PacketHeaderCodec::serialize(header, ByteOrder::BigEndian, out);

   ASSERT_EQ(out.size(), 8u);
   EXPECT_EQ(readU32BE(out.data()), 0x10000002u);
   EXPECT_EQ(readU32BE(out.data() + 4), 0xDEADCAFEu);
}

TEST(PacketHeaderTest, SerializePacketCount)
{
   PacketHeader header;
   header.packetType = PacketType::IFDataWithStreamId;
   header.streamId   = 0;
   header.packetCount = 7;
   header.packetSize = 2;

   std::vector<uint8_t> out;
   PacketHeaderCodec::serialize(header, ByteOrder::BigEndian, out);

   uint32_t word0 = readU32BE(out.data());
   EXPECT_EQ((word0 >> 16) & 0xFu, 7u);
}

// ============================================================================
// Round-Trip
// ============================================================================

TEST(PacketHeaderTest, RoundTrip_FullHeader_BigEndian)
{
   PacketHeader original;
   original.packetType          = PacketType::IFDataWithStreamId;
   original.classIdPresent      = true;
   original.trailerPresent      = true;
   original.tsiType             = TSI::GPS;
   original.tsfType             = TSF::SampleCount;
   original.packetCount         = 12;
   original.streamId            = 0xFEEDFACE;
   original.classIdOUI          = 0xABCDEF;
   original.informationClassCode = 0x1234;
   original.packetClassCode     = 0x5678;
   original.integerTimestamp    = 1000000;
   original.fractionalTimestamp = 999999999999ULL;

   original.packetSize = static_cast<uint16_t>(
      PacketHeaderCodec::sizeInWords(original));

   std::vector<uint8_t> serialized;
   PacketHeaderCodec::serialize(original, ByteOrder::BigEndian, serialized);
   ASSERT_EQ(serialized.size(), original.packetSize * 4u);

   PacketHeader parsed;
   size_t headerBytes = 0;
   ASSERT_TRUE(PacketHeaderCodec::parse(
      serialized.data(), serialized.size(),
      ByteOrder::BigEndian, parsed, headerBytes));

   EXPECT_EQ(parsed.packetType, original.packetType);
   EXPECT_EQ(parsed.classIdPresent, original.classIdPresent);
   EXPECT_EQ(parsed.trailerPresent, original.trailerPresent);
   EXPECT_EQ(parsed.tsiType, original.tsiType);
   EXPECT_EQ(parsed.tsfType, original.tsfType);
   EXPECT_EQ(parsed.packetCount, original.packetCount);
   EXPECT_EQ(parsed.packetSize, original.packetSize);
   ASSERT_TRUE(parsed.streamId.has_value());
   EXPECT_EQ(parsed.streamId.value(), original.streamId.value());
   ASSERT_TRUE(parsed.classIdOUI.has_value());
   EXPECT_EQ(parsed.classIdOUI.value(), original.classIdOUI.value());
   ASSERT_TRUE(parsed.informationClassCode.has_value());
   EXPECT_EQ(parsed.informationClassCode.value(),
             original.informationClassCode.value());
   ASSERT_TRUE(parsed.packetClassCode.has_value());
   EXPECT_EQ(parsed.packetClassCode.value(),
             original.packetClassCode.value());
   ASSERT_TRUE(parsed.integerTimestamp.has_value());
   EXPECT_EQ(parsed.integerTimestamp.value(),
             original.integerTimestamp.value());
   ASSERT_TRUE(parsed.fractionalTimestamp.has_value());
   EXPECT_EQ(parsed.fractionalTimestamp.value(),
             original.fractionalTimestamp.value());
}

TEST(PacketHeaderTest, RoundTrip_FullHeader_LittleEndian)
{
   PacketHeader original;
   original.packetType          = PacketType::ExtDataWithStreamId;
   original.classIdPresent      = false;
   original.trailerPresent      = false;
   original.tsiType             = TSI::UTC;
   original.tsfType             = TSF::FreeRunning;
   original.packetCount         = 5;
   original.streamId            = 0x87654321;
   original.integerTimestamp    = 42;
   original.fractionalTimestamp = 123456789ULL;

   original.packetSize = static_cast<uint16_t>(
      PacketHeaderCodec::sizeInWords(original));

   std::vector<uint8_t> serialized;
   PacketHeaderCodec::serialize(original, ByteOrder::LittleEndian, serialized);

   PacketHeader parsed;
   size_t headerBytes = 0;
   ASSERT_TRUE(PacketHeaderCodec::parse(
      serialized.data(), serialized.size(),
      ByteOrder::LittleEndian, parsed, headerBytes));

   EXPECT_EQ(parsed.packetType, original.packetType);
   EXPECT_EQ(parsed.tsiType, original.tsiType);
   EXPECT_EQ(parsed.tsfType, original.tsfType);
   EXPECT_EQ(parsed.packetCount, original.packetCount);
   ASSERT_TRUE(parsed.streamId.has_value());
   EXPECT_EQ(parsed.streamId.value(), original.streamId.value());
   ASSERT_TRUE(parsed.integerTimestamp.has_value());
   EXPECT_EQ(parsed.integerTimestamp.value(),
             original.integerTimestamp.value());
   ASSERT_TRUE(parsed.fractionalTimestamp.has_value());
   EXPECT_EQ(parsed.fractionalTimestamp.value(),
             original.fractionalTimestamp.value());
}

// ============================================================================
// Size Calculations
// ============================================================================

TEST(PacketHeaderTest, SizeInWords_Minimal)
{
   PacketHeader header;
   header.packetType = PacketType::IFDataWithoutStreamId;
   EXPECT_EQ(PacketHeaderCodec::sizeInWords(header), 1u);
}

TEST(PacketHeaderTest, SizeInWords_WithStreamId)
{
   PacketHeader header;
   header.packetType = PacketType::IFDataWithStreamId;
   EXPECT_EQ(PacketHeaderCodec::sizeInWords(header), 2u);
}

TEST(PacketHeaderTest, SizeInWords_Full)
{
   PacketHeader header;
   header.packetType    = PacketType::IFDataWithStreamId;
   header.classIdPresent = true;
   header.tsiType       = TSI::UTC;
   header.tsfType       = TSF::RealTime;
   // 1 (word0) + 1 (streamId) + 2 (classId) + 1 (intTS) + 2 (fracTS) = 7
   EXPECT_EQ(PacketHeaderCodec::sizeInWords(header), 7u);
}

TEST(PacketHeaderTest, SizeInBytes)
{
   PacketHeader header;
   header.packetType = PacketType::IFDataWithStreamId;
   EXPECT_EQ(PacketHeaderCodec::sizeInBytes(header), 8u);
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST(PacketHeaderTest, HasStreamId)
{
   EXPECT_FALSE(hasStreamId(PacketType::IFDataWithoutStreamId));
   EXPECT_TRUE(hasStreamId(PacketType::IFDataWithStreamId));
   EXPECT_FALSE(hasStreamId(PacketType::ExtDataWithoutStreamId));
   EXPECT_TRUE(hasStreamId(PacketType::ExtDataWithStreamId));
   EXPECT_TRUE(hasStreamId(PacketType::IFContext));
   EXPECT_TRUE(hasStreamId(PacketType::ExtContext));
}

TEST(PacketHeaderTest, IsDataPacket)
{
   EXPECT_TRUE(isDataPacket(PacketType::IFDataWithoutStreamId));
   EXPECT_TRUE(isDataPacket(PacketType::IFDataWithStreamId));
   EXPECT_TRUE(isDataPacket(PacketType::ExtDataWithoutStreamId));
   EXPECT_TRUE(isDataPacket(PacketType::ExtDataWithStreamId));
   EXPECT_FALSE(isDataPacket(PacketType::IFContext));
   EXPECT_FALSE(isDataPacket(PacketType::ExtContext));
}

TEST(PacketHeaderTest, IsContextPacket)
{
   EXPECT_FALSE(isContextPacket(PacketType::IFDataWithoutStreamId));
   EXPECT_FALSE(isContextPacket(PacketType::IFDataWithStreamId));
   EXPECT_TRUE(isContextPacket(PacketType::IFContext));
   EXPECT_TRUE(isContextPacket(PacketType::ExtContext));
}
