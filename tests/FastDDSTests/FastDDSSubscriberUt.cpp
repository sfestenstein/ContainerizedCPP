/**
 * @file FastDDSSubscriberUt.cpp
 * @brief Unit tests for FastDDSSubscriber.
 *
 * Tests construction, subscribe, and end-to-end pub/sub within
 * an isolated DDS domain.
 */

#include <gtest/gtest.h>
#include "FastDDS/FastDDSPublisher.h"
#include "FastDDS/FastDDSSubscriber.h"
#include "FastDDS/FastDDSTopicConfig.h"

#include "FastDDSTestMessage.h"
#include "FastDDSTestMessagePubSubTypes.h"

#include <atomic>
#include <chrono>
#include <thread>

using fastdds_test::FastDDSTestMessage;
using fastdds_test::FastDDSTestMessagePubSubType;
using PubType = FastDDS::FastDDSPublisher<FastDDSTestMessage, FastDDSTestMessagePubSubType>;
using SubType = FastDDS::FastDDSSubscriber<FastDDSTestMessage, FastDDSTestMessagePubSubType>;

// Use a high domain ID to isolate test traffic
static constexpr uint32_t TEST_DOMAIN_ID = 88;

class FastDDSSubscriberTest : public ::testing::Test
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

TEST_F(FastDDSSubscriberTest, Construction_ValidDomain_Succeeds)
{
   EXPECT_NO_THROW({
      SubType sub(TEST_DOMAIN_ID, makeEntry("AnyTopic"), "TestSub");
   });
}

TEST_F(FastDDSSubscriberTest, Subscribe_SingleTopic_NoThrow)
{
   SubType sub(TEST_DOMAIN_ID, makeEntry("TestSubTopic"), "SubTest");

   EXPECT_NO_THROW(
      sub.subscribe([](const FastDDSTestMessage &) {})
   );
}

TEST_F(FastDDSSubscriberTest, TopicEntry_ReturnsConfiguredEntry)
{
   SubType sub(TEST_DOMAIN_ID, makeEntry("CheckTopic"), "EntrySub");

   EXPECT_EQ(sub.topicEntry().topicName, "CheckTopic");
}

TEST_F(FastDDSSubscriberTest, EndToEnd_ReceivesData)
{
   // Use a unique domain to avoid cross-test interference
   constexpr uint32_t E2E_DOMAIN = 87;
   auto entry = makeEntry("E2ETestTopic");

   std::atomic<int> receivedCount{0};
   std::string receivedId;

   SubType sub(E2E_DOMAIN, entry, "E2ESub");
   sub.subscribe(
      [&](const FastDDSTestMessage &msg)
      {
         receivedId = msg.id();
         receivedCount.fetch_add(1);
      });

   // Give the subscriber's participant time to be ready for discovery
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   PubType pub(E2E_DOMAIN, entry, "E2EPub");

   // Give publisher/subscriber participants time to discover each other
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   FastDDSTestMessage msg;
   msg.id("e2e-message");
   msg.value(42.0);
   msg.sequence_number(1);

   pub.publish(msg);

   // Wait for delivery (DDS discovery + delivery can take a moment)
   auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
   while (receivedCount.load() == 0 &&
          std::chrono::steady_clock::now() < deadline)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }

   EXPECT_GE(receivedCount.load(), 1);
   EXPECT_EQ(receivedId, "e2e-message");
}
