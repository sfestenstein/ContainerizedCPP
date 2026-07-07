/**
 * @file ProtoTopicConfigUt.cpp
 * @brief Unit tests for ProtoTopicConfig.
 *
 * Validates the constexpr enum-to-string mapping and enforces that
 * every TopicId is associated with a valid, default-constructible
 * protobuf Msg type that can round-trip through serialization.
 */

#include <gtest/gtest.h>

#include "ProtoTopicConfig.h"

#include <sensor_reading.pb.h>
#include <sensor_data_batch.pb.h>
#include <command.pb.h>
#include <command_response.pb.h>
#include <application_config.pb.h>

#include <algorithm>

using namespace ProtoMessages;

// ── constexpr mapping tests ─────────────────────────────────────────────

TEST(ProtoTopicConfigTest, TopicName_AllIds_ReturnNonEmpty)
{
   for (std::size_t i = 0; i < static_cast<std::size_t>(TopicId::COUNT); ++i)
   {
      auto id = static_cast<TopicId>(i);
      EXPECT_FALSE(topicName(id).empty())
         << "TopicId " << i << " has no name in TOPIC_REGISTRY";
   }
}

TEST(ProtoTopicConfigTest, TopicId_RoundTrip_AllIds)
{
   for (std::size_t i = 0; i < static_cast<std::size_t>(TopicId::COUNT); ++i)
   {
      auto id = static_cast<TopicId>(i);
      auto name = topicName(id);
      auto resolved = topicId(name);
      ASSERT_TRUE(resolved.has_value()) << "topicId(\"" << name << "\") returned nullopt";
      EXPECT_EQ(resolved.value(), id);
   }
}

TEST(ProtoTopicConfigTest, TopicId_UnknownName_ReturnsNullopt)
{
   EXPECT_FALSE(topicId("DoesNotExist").has_value());
}

TEST(ProtoTopicConfigTest, AllTopicNames_ReturnsCorrectCount)
{
   auto names = allTopicNames();
   EXPECT_EQ(names.size(), static_cast<std::size_t>(TopicId::COUNT));
}

TEST(ProtoTopicConfigTest, AllTopicNames_NoDuplicates)
{
   auto names = allTopicNames();
   std::ranges::sort(names);
   EXPECT_EQ(std::ranges::adjacent_find(names), names.end())
      << "Duplicate topic names found in TOPIC_REGISTRY";
}

TEST(ProtoTopicConfigTest, RegistryIds_NoDuplicates)
{
   std::vector<TopicId> ids;
   ids.reserve(TOPIC_REGISTRY.size());
   for (const auto &entry : TOPIC_REGISTRY)
   {
      ids.push_back(entry.id);
   }
   std::ranges::sort(ids);
   EXPECT_EQ(std::ranges::adjacent_find(ids), ids.end())
      << "Duplicate TopicId values found in TOPIC_REGISTRY";
}

// ── type-safety enforcement tests ───────────────────────────────────────
// Each test verifies that the expected proto Msg type can be default-
// constructed and round-tripped through serialization for its TopicId.

namespace
{

template <typename MsgType>
void verifyRoundTrip(TopicId id)
{
   auto name = topicName(id);
   SCOPED_TRACE(std::string("TopicId -> ") + std::string(name));

   MsgType original;
   original.mutable_header()->set_message_type(std::string(name));
   original.mutable_header()->set_timestamp_ns(1234567890);

   std::string serialized;
   ASSERT_TRUE(original.SerializeToString(&serialized))
      << "Failed to serialize " << name;

   MsgType deserialized;
   ASSERT_TRUE(deserialized.ParseFromString(serialized))
      << "Failed to deserialize " << name;

   EXPECT_EQ(deserialized.header().message_type(), std::string(name));
   EXPECT_EQ(deserialized.header().timestamp_ns(), 1234567890);
}

} // anonymous namespace

TEST(ProtoTopicConfigTest, TypeSafety_SensorReading_RoundTrip)
{
   verifyRoundTrip<messages::SensorReadingMsg>(TopicId::SensorReading);
}

TEST(ProtoTopicConfigTest, TypeSafety_SensorDataBatch_RoundTrip)
{
   verifyRoundTrip<messages::SensorDataBatchMsg>(TopicId::SensorDataBatch);
}

TEST(ProtoTopicConfigTest, TypeSafety_Command_RoundTrip)
{
   verifyRoundTrip<messages::CommandMsg>(TopicId::Command);
}

TEST(ProtoTopicConfigTest, TypeSafety_CommandResponse_RoundTrip)
{
   verifyRoundTrip<messages::CommandResponseMsg>(TopicId::CommandResponse);
}

TEST(ProtoTopicConfigTest, TypeSafety_ApplicationConfig_RoundTrip)
{
   verifyRoundTrip<messages::ApplicationConfigMsg>(TopicId::ApplicationConfig);
}
