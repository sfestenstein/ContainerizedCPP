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

#include "SensorData.hpp"
#include "TrackData.hpp"

#include <atomic>
#include <chrono>
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
      CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(
         TEST_DOMAIN_ID, makeEntry("AnyTopic"), "TestSub");
   });
}

TEST_F(DDSSubscriberTest, Subscribe_SingleTopic_NoThrow)
{
   CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(
      TEST_DOMAIN_ID, makeEntry("TestSubTopic"), "SubTest");

   EXPECT_NO_THROW(
      sub.subscribe([](const dds_messages::SensorReading &) {})
   );
}

TEST_F(DDSSubscriberTest, StartStop_NoSubscriptions_NoThrow)
{
   CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(
      TEST_DOMAIN_ID, makeEntry("AnyTopic"), "StartStopTest");

   EXPECT_NO_THROW(sub.start());
   EXPECT_TRUE(sub.isRunning());

   EXPECT_NO_THROW(sub.stop());
   EXPECT_FALSE(sub.isRunning());
}

TEST_F(DDSSubscriberTest, StartStop_DoubleStart_Idempotent)
{
   CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(
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
   CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(
      TEST_DOMAIN_ID, makeEntry("AnyTopic"), "DoubleStopTest");

   sub.start();
   sub.stop();
   EXPECT_FALSE(sub.isRunning());

   // Second stop should be a no-op
   EXPECT_NO_THROW(sub.stop());
   EXPECT_FALSE(sub.isRunning());
}

TEST_F(DDSSubscriberTest, EndToEnd_SensorReading_ReceivesData)
{
   // Use a unique domain to avoid cross-test interference
   constexpr uint32_t E2E_DOMAIN = 97;
   auto entry = makeEntry("E2ESensorTopic");

   std::atomic<int> receivedCount{0};
   std::string receivedSensorId;

   CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(
      E2E_DOMAIN, entry, "E2ESub");
   sub.subscribe(
      [&](const dds_messages::SensorReading &msg)
      {
         receivedSensorId = msg.sensor_id();
         receivedCount.fetch_add(1);
      });
   sub.start();

   // Give participant time to discover
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   CycloneDDS::DDSPublisher<dds_messages::SensorReading> pub(
      E2E_DOMAIN, entry, "E2EPub");

   // Give publisher participant time to discover the subscriber
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   dds_messages::SensorReading msg;
   msg.sensor_id("e2e-sensor");
   msg.sensor_name("E2E Test");
   msg.value(42.0);
   msg.unit("celsius");
   msg.timestamp_ms(12345);
   msg.quality(100);
   msg.status(dds_messages::SensorStatus::SENSOR_ONLINE);

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
   EXPECT_EQ(receivedSensorId, "e2e-sensor");
}

TEST_F(DDSSubscriberTest, EndToEnd_TrackUpdate_ReceivesData)
{
   constexpr uint32_t E2E_DOMAIN = 96;
   auto entry = makeEntry("E2ETrackTopic");

   std::atomic<int> receivedCount{0};
   std::string receivedTrackId;

   CycloneDDS::DDSSubscriber<dds_messages::TrackUpdate> sub(
      E2E_DOMAIN, entry, "TrackE2ESub");
   sub.subscribe(
      [&](const dds_messages::TrackUpdate &msg)
      {
         receivedTrackId = msg.track_id();
         receivedCount.fetch_add(1);
      });
   sub.start();

   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   CycloneDDS::DDSPublisher<dds_messages::TrackUpdate> pub(
      E2E_DOMAIN, entry, "TrackE2EPub");

   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   dds_messages::TrackUpdate msg;
   msg.track_id("track-e2e");
   msg.track_name("E2E Track");
   msg.latitude(37.0);
   msg.longitude(-122.0);
   msg.altitude(5000.0);
   msg.heading(180.0);
   msg.speed(300.0);
   msg.classification(dds_messages::TrackClassification::TRACK_NEUTRAL);
   msg.timestamp_ms(99999);
   msg.update_number(1);
   msg.confidence(0.9);

   pub.publish(msg);

   auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
   while (receivedCount.load() == 0 &&
          std::chrono::steady_clock::now() < deadline)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }

   sub.stop();

   EXPECT_GE(receivedCount.load(), 1);
   EXPECT_EQ(receivedTrackId, "track-e2e");
}

TEST_F(DDSSubscriberTest, TopicEntry_ReturnsConfiguredEntry)
{
   CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(
      TEST_DOMAIN_ID, makeEntry("CheckTopic"), "EntrySub");

   EXPECT_EQ(sub.topicEntry().topicName, "CheckTopic");
}
