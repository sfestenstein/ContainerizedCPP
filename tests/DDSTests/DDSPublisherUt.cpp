/**
 * @file DDSPublisherUt.cpp
 * @brief Unit tests for DDSPublisher.
 *
 * Tests basic construction and publish operations using a local-only
 * DDS domain (high domain ID to avoid conflicts).
 */

#include <gtest/gtest.h>
#include "CycloneDDS/DDSPublisher.h"
#include "CycloneDDS/DDSTopicConfig.h"
#include "Observability/FakeInterfaceMetrics.h"
#include "Observability/ScopedMetrics.h"

#include "TestMessage.hpp"

#include <memory>

// Use a high domain ID to isolate test traffic
static constexpr uint32_t TEST_DOMAIN_ID = 99;

class DDSPublisherTest : public ::testing::Test
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

TEST_F(DDSPublisherTest, Construction_ValidDomain_Succeeds)
{
   EXPECT_NO_THROW({
      CycloneDDS::DDSPublisher<dds_test::TestMessage> pub(
         TEST_DOMAIN_ID, makeEntry("AnyTopic"), "TestPub");
   });
}

TEST_F(DDSPublisherTest, Publish_TestMessage_NoThrow)
{
   CycloneDDS::DDSPublisher<dds_test::TestMessage> pub(
      TEST_DOMAIN_ID, makeEntry("TestSensorTopic"), "SensorPub");

   dds_test::TestMessage msg;
   msg.id("test-sensor");
   msg.name("Test Temperature");
   msg.value(22.5);
   msg.timestamp_ms(1000);

   EXPECT_NO_THROW(pub.publish(msg));
}

TEST_F(DDSPublisherTest, Publish_MultipleSamples_SameTopic)
{
   CycloneDDS::DDSPublisher<dds_test::TestMessage> pub(
      TEST_DOMAIN_ID, makeEntry("TestBatchTopic"), "MultiPub");

   for (int i = 0; i < 10; ++i)
   {
      dds_test::TestMessage msg;
      msg.id("sensor-" + std::to_string(i));
      msg.value(static_cast<double>(i) * 1.5);
      msg.timestamp_ms(static_cast<int64_t>(i) * 100);

      EXPECT_NO_THROW(pub.publish(msg));
   }
}

TEST_F(DDSPublisherTest, TopicEntry_ReturnsConfiguredEntry)
{
   CycloneDDS::DDSPublisher<dds_test::TestMessage> pub(
      TEST_DOMAIN_ID, makeEntry("CheckTopic"), "EntryPub");

   EXPECT_EQ(pub.topicEntry().topicName, "CheckTopic");
}

TEST_F(DDSPublisherTest, Publish_RecordsMetrics_ViaGlobalRegistry)
{
   // DDSPublisher takes no metrics constructor argument -- it reaches
   // Observability::metrics() directly (see DESIGN.md), so tests install a
   // fake into that global registry for the scope of this test rather than
   // injecting one.
   auto fake = std::make_shared<Observability::FakeInterfaceMetrics>();
   Observability::ScopedMetrics guard(fake);

   CycloneDDS::DDSPublisher<dds_test::TestMessage> pub(
      TEST_DOMAIN_ID, makeEntry("MetricsTopic"), "MetricsPub");

   dds_test::TestMessage msg;
   msg.id("metrics-sensor");
   msg.name("Metrics Test");
   msg.value(1.0);
   msg.timestamp_ms(1);

   pub.publish(msg);

   ASSERT_EQ(fake->sent.size(), 1u);
   EXPECT_EQ(fake->sent[0].interfaceName, "MetricsPub");
   EXPECT_EQ(fake->sent[0].type, Observability::InterfaceType::DDS_INTERFACE);
   EXPECT_EQ(fake->sent[0].topic, "MetricsTopic");
   EXPECT_EQ(fake->sent[0].bytes, sizeof(dds_test::TestMessage));
   EXPECT_TRUE(fake->received.empty());
}
