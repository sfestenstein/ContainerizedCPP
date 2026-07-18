#include "RawSampleJsonCodec.h"

#include <gtest/gtest.h>

using Omniscope::RawSampleJsonCodec::fromJson;
using Omniscope::RawSampleJsonCodec::toJson;

namespace
{

DdsCore::RawSample makeSample()
{
   DdsCore::RawSample sample;
   sample.typeName     = "radar_demo::RadarTrack";
   sample.bytes         = {0x00, 0x01, 0xAB, 0xFF};
   sample.reliability   = "reliable";
   sample.durability    = "transient_local";
   sample.historyDepth  = 5;
   return sample;
}

} // namespace

TEST(RawSampleJsonCodecTest, ToJson_EncodesBytesAsLowercaseHex)
{
   auto json = toJson(makeSample());
   EXPECT_NE(json.find(R"("raw_cdr":"0001abff")"), std::string::npos);
   EXPECT_NE(json.find(R"("byte_count":4)"), std::string::npos);
   EXPECT_NE(json.find(R"("type_name":"radar_demo::RadarTrack")"), std::string::npos);
   EXPECT_NE(json.find(R"("reliability":"reliable")"), std::string::npos);
   EXPECT_NE(json.find(R"("durability":"transient_local")"), std::string::npos);
   EXPECT_NE(json.find(R"("history_depth":5)"), std::string::npos);
}

TEST(RawSampleJsonCodecTest, ToJson_EmptyBytes_ProducesEmptyHex)
{
   DdsCore::RawSample sample;
   sample.reliability = "best_effort";
   sample.durability  = "volatile";
   sample.historyDepth = 1;

   auto json = toJson(sample);
   EXPECT_NE(json.find(R"("raw_cdr":"")"), std::string::npos);
   EXPECT_NE(json.find(R"("byte_count":0)"), std::string::npos);
}

TEST(RawSampleJsonCodecTest, RoundTrip_ToJsonThenFromJson_PreservesAllFields)
{
   auto original = makeSample();
   auto decoded = fromJson(toJson(original));

   ASSERT_TRUE(decoded.has_value());
   EXPECT_EQ(decoded->typeName, original.typeName);
   EXPECT_EQ(decoded->bytes, original.bytes);
   EXPECT_EQ(decoded->reliability, original.reliability);
   EXPECT_EQ(decoded->durability, original.durability);
   EXPECT_EQ(decoded->historyDepth, original.historyDepth);
}

TEST(RawSampleJsonCodecTest, FromJson_InvalidJson_ReturnsNullopt)
{
   EXPECT_FALSE(fromJson("not json").has_value());
   EXPECT_FALSE(fromJson("").has_value());
}

TEST(RawSampleJsonCodecTest, FromJson_MissingRawCdr_ReturnsNullopt)
{
   EXPECT_FALSE(fromJson(R"({"type_name":"Foo"})").has_value());
}

TEST(RawSampleJsonCodecTest, FromJson_OddLengthHex_ReturnsNullopt)
{
   EXPECT_FALSE(fromJson(R"({"raw_cdr":"abc"})").has_value());
}

TEST(RawSampleJsonCodecTest, FromJson_NonHexCharacters_ReturnsNullopt)
{
   EXPECT_FALSE(fromJson(R"({"raw_cdr":"zzzz"})").has_value());
}

TEST(RawSampleJsonCodecTest, FromJson_MissingTypeNameAndQos_UsesDefaults)
{
   auto decoded = fromJson(R"({"raw_cdr":"0a0b"})");
   ASSERT_TRUE(decoded.has_value());
   EXPECT_EQ(decoded->typeName, "");
   EXPECT_EQ(decoded->reliability, "reliable");
   EXPECT_EQ(decoded->durability, "volatile");
   EXPECT_EQ(decoded->historyDepth, 1);
   EXPECT_EQ(decoded->bytes, (std::vector<uint8_t>{0x0a, 0x0b}));
}

TEST(RawSampleJsonCodecTest, FromJson_PartialQos_FillsProvidedFieldsOnly)
{
   auto decoded = fromJson(R"({"raw_cdr":"ff","qos":{"reliability":"best_effort"}})");
   ASSERT_TRUE(decoded.has_value());
   EXPECT_EQ(decoded->reliability, "best_effort");
   EXPECT_EQ(decoded->durability, "volatile"); // unspecified, default retained
   EXPECT_EQ(decoded->historyDepth, 1);
}
