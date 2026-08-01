/**
 * @file DDSSubscriberUt.cpp
 * @brief Unit tests for DDSSubscriber.
 *
 * Tests construction, subscribe, and end-to-end pub/sub within
 * an isolated DDS domain.
 */

#include <gtest/gtest.h>
#include "CycloneDDS/DDSPublisher.h"
#include "CycloneDDS/DDSSubscriber.h"
#include "CycloneDDS/DDSTopicConfig.h"
#include "Observability/FakeInterfaceMetrics.h"
#include "Observability/ScopedMetrics.h"

#include "TestMessage.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

// Use a high domain ID to isolate test traffic
static constexpr uint32_t TEST_DOMAIN_ID = 98;

class DDSSubscriberTest : public ::testing::Test
{
protected:
   void SetUp() override {}
   void TearDown() override {}

   /**
    * @brief Build a TopicEntry with default QoS for the given topic name.
    */
   static CycloneDDS::TopicEntry makeEntry(const std::string &topic)
   {
      return
      {
         .topicName = topic,
         .writerQos = dds::pub::qos::DataWriterQos{},
         .readerQos = dds::sub::qos::DataReaderQos{}
     };
   }
};

TEST_F(DDSSubscriberTest, Construction_ValidDomain_Succeeds)
{
   EXPECT_NO_THROW({
      CycloneDDS::DDSSubscriber<dds_test::TestMessage> sub(
         TEST_DOMAIN_ID, makeEntry("AnyTopic"), "TestSub");
   });
}

TEST_F(DDSSubscriberTest, Subscribe_SingleTopic_NoThrow)
{
   CycloneDDS::DDSSubscriber<dds_test::TestMessage> sub(
      TEST_DOMAIN_ID, makeEntry("TestSubTopic"), "SubTest");

   EXPECT_NO_THROW(
      sub.subscribe([](const dds_test::TestMessage &) {})
   );
}

TEST_F(DDSSubscriberTest, StartStop_NoSubscriptions_NoThrow)
{
   CycloneDDS::DDSSubscriber<dds_test::TestMessage> sub(
      TEST_DOMAIN_ID, makeEntry("AnyTopic"), "StartStopTest");

   EXPECT_NO_THROW(sub.start());
   EXPECT_TRUE(sub.isRunning());

   EXPECT_NO_THROW(sub.stop());
   EXPECT_FALSE(sub.isRunning());
}

TEST_F(DDSSubscriberTest, StartStop_DoubleStart_Idempotent)
{
   CycloneDDS::DDSSubscriber<dds_test::TestMessage> sub(
      TEST_DOMAIN_ID, makeEntry("AnyTopic"), "DoubleStartTest");

   sub.start();
   EXPECT_TRUE(sub.isRunning());

   // Second start should be a no-op
   sub.start();
   EXPECT_TRUE(sub.isRunning());

   sub.stop();
   EXPECT_FALSE(sub.isRunning());
}

TEST_F(DDSSubscriberTest, StartStop_DoubleStop_Idempotent)
{
   CycloneDDS::DDSSubscriber<dds_test::TestMessage> sub(
      TEST_DOMAIN_ID, makeEntry("AnyTopic"), "DoubleStopTest");

   sub.start();
   sub.stop();
   EXPECT_FALSE(sub.isRunning());

   // Second stop should be a no-op
   EXPECT_NO_THROW(sub.stop());
   EXPECT_FALSE(sub.isRunning());
}

TEST_F(DDSSubscriberTest, EndToEnd_TestMessage_ReceivesData)
{
   // Use a unique domain to avoid cross-test interference
   constexpr uint32_t E2E_DOMAIN = 97;
   auto entry = makeEntry("E2ETestTopic");

   std::atomic<int> receivedCount{0};
   std::string receivedId;

   CycloneDDS::DDSSubscriber<dds_test::TestMessage> sub(
      E2E_DOMAIN, entry, "E2ESub");
   sub.subscribe(
      [&](const dds_test::TestMessage &msg)
      {
         receivedId = msg.id();
         receivedCount.fetch_add(1);
      });
   sub.start();

   // Give participant time to discover
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   CycloneDDS::DDSPublisher<dds_test::TestMessage> pub(
      E2E_DOMAIN, entry, "E2EPub");

   // Give publisher participant time to discover the subscriber
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   dds_test::TestMessage msg;
   msg.id("e2e-sensor");
   msg.name("E2E Test");
   msg.value(42.0);
   msg.timestamp_ms(12345);

   pub.publish(msg);

   // Wait for delivery (DDS discovery + delivery can take a moment)
   auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
   while (receivedCount.load() == 0 &&
          std::chrono::steady_clock::now() < deadline)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }

   sub.stop();

   EXPECT_GE(receivedCount.load(), 1);
   EXPECT_EQ(receivedId, "e2e-sensor");
}

TEST_F(DDSSubscriberTest, TopicEntry_ReturnsConfiguredEntry)
{
   CycloneDDS::DDSSubscriber<dds_test::TestMessage> sub(
      TEST_DOMAIN_ID, makeEntry("CheckTopic"), "EntrySub");

   EXPECT_EQ(sub.topicEntry().topicName, "CheckTopic");
}

TEST_F(DDSSubscriberTest, EndToEnd_RecordsMetrics_LatenciesStayEmptyForHeaderlessMessage)
{
   // DDSSubscriber takes no metrics constructor argument -- it reaches
   // Observability::metrics() directly (see DESIGN.md), so tests install a
   // fake into that global registry for the scope of this test rather than
   // injecting one.
   auto fake = std::make_shared<Observability::FakeInterfaceMetrics>();
   Observability::ScopedMetrics guard(fake);

   constexpr uint32_t METRICS_DOMAIN = 96;
   auto entry = makeEntry("MetricsE2ETopic");

   std::atomic<int> receivedCount{0};

   CycloneDDS::DDSSubscriber<dds_test::TestMessage> sub(METRICS_DOMAIN, entry, "MetricsE2ESub");
   sub.subscribe([&](const dds_test::TestMessage &) { receivedCount.fetch_add(1); });
   sub.start();

   // Give participant time to discover.
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   CycloneDDS::DDSPublisher<dds_test::TestMessage> pub(METRICS_DOMAIN, entry, "MetricsE2EPub");

   // Give publisher participant time to discover the subscriber.
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   dds_test::TestMessage msg;
   msg.id("metrics-e2e-sensor");
   msg.name("Metrics E2E Test");
   msg.value(1.0);
   msg.timestamp_ms(1);

   pub.publish(msg);

   auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
   while (receivedCount.load() == 0 && std::chrono::steady_clock::now() < deadline)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }

   sub.stop();

   ASSERT_GE(receivedCount.load(), 1);
   ASSERT_FALSE(fake->received.empty());
   EXPECT_EQ(fake->received[0].interfaceName, "MetricsE2ESub");
   EXPECT_EQ(fake->received[0].type, Observability::InterfaceType::DDS_INTERFACE);
   EXPECT_EQ(fake->received[0].topic, "MetricsE2ETopic");

   // dds_test::TestMessage has no header/timestamp field, so the compile-time
   // trait in DDSMessageTraits.h must degrade safely: no latency ever gets
   // recorded for a message type that doesn't opt in.
   EXPECT_TRUE(fake->latencies.empty());
}
