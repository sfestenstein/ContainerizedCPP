/**
 * @file FastDDSPublisherUt.cpp
 * @brief Unit tests for FastDDSPublisher.
 *
 * Tests basic construction and publish operations using a local-only
 * DDS domain (high domain ID to avoid conflicts).
 */

#include <gtest/gtest.h>
#include "FastDDS/FastDDSPublisher.h"
#include "FastDDS/FastDDSTopicConfig.h"

#include "FastDDSTestMessage.h"
#include "FastDDSTestMessagePubSubTypes.h"

using fastdds_test::FastDDSTestMessage;
using fastdds_test::FastDDSTestMessagePubSubType;
using PubType = FastDDS::FastDDSPublisher<FastDDSTestMessage, FastDDSTestMessagePubSubType>;

// Use a high domain ID to isolate test traffic
static constexpr uint32_t TEST_DOMAIN_ID = 89;

class FastDDSPublisherTest : public ::testing::Test
{
protected:
   /**
    * @brief Build a TopicEntry with default QoS for the given topic name.
    */
   static FastDDS::TopicEntry makeEntry(const std::string &topic)
   {
      return
      {
         .topicName = topic,
         .writerQos = eprosima::fastdds::dds::DataWriterQos{},
         .readerQos = eprosima::fastdds::dds::DataReaderQos{}
      };
   }
};

TEST_F(FastDDSPublisherTest, Construction_ValidDomain_Succeeds)
{
   EXPECT_NO_THROW({
      PubType pub(TEST_DOMAIN_ID, makeEntry("AnyTopic"), "TestPub");
   });
}

TEST_F(FastDDSPublisherTest, Publish_Message_NoThrow)
{
   PubType pub(TEST_DOMAIN_ID, makeEntry("TestMessageTopic"), "MessagePub");

   FastDDSTestMessage msg;
   msg.id("test-id");
   msg.value(22.5);
   msg.sequence_number(1);

   EXPECT_NO_THROW(pub.publish(msg));
}

TEST_F(FastDDSPublisherTest, Publish_MultipleSamples_SameTopic)
{
   PubType pub(TEST_DOMAIN_ID, makeEntry("TestBatchTopic"), "MultiPub");

   for (int i = 0; i < 10; ++i)
   {
      FastDDSTestMessage msg;
      msg.id("id-" + std::to_string(i));
      msg.value(static_cast<double>(i) * 1.5);
      msg.sequence_number(i);

      EXPECT_NO_THROW(pub.publish(msg));
   }
}

TEST_F(FastDDSPublisherTest, TopicEntry_ReturnsConfiguredEntry)
{
   PubType pub(TEST_DOMAIN_ID, makeEntry("CheckTopic"), "EntryPub");

   EXPECT_EQ(pub.topicEntry().topicName, "CheckTopic");
}
