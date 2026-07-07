/**
 * @file HighBandwidthSubscriberUt.cpp
 * @brief Unit tests for HighBandwidthSubscriber class.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <cstring>
#include <thread>

#include "GeneralLogger.h"
#include "HighBandwidthSubscriber.h"
#include "HighBandwidthPublisher.h"  // For FragmentHeader


class HighBandwidthSubscriberTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      // Use a non-default port to avoid conflicts with other tests
      _testPort = 15671;
      _testMulticastAddr = "239.192.100.2";
   }

   void TearDown() override
   {
   }

   // Helper to access private members
   static bool isRunning(const HighBandwidthSubscriber& sub) { return sub._running.load(); }
   static bool shouldStop(const HighBandwidthSubscriber& sub) { return sub._shouldStop.load(); }
   static int getSocket(const HighBandwidthSubscriber& sub) { return sub._socket; }
   static size_t getHandlerCount(HighBandwidthSubscriber& sub)
   {
      const std::lock_guard<std::mutex> lock(sub._handlersMutex);
      return sub._handlers.size();
   }
   static size_t getPartialMessageCount(HighBandwidthSubscriber& sub)
   {
      const std::lock_guard<std::mutex> lock(sub._reassemblyMutex);
      return sub._partialMessages.size();
   }

   // Helper to call private methods
   static void callCleanupStaleMessages(HighBandwidthSubscriber& sub) { sub.cleanupStaleMessages(); }
   static void callProcessFragment(HighBandwidthSubscriber& sub, const uint8_t* data, size_t len)
   {
      sub.processFragment(data, len);
   }
   static void callDeliverMessage(HighBandwidthSubscriber& sub, const std::string& topic, const std::string& payload)
   {
      sub.deliverMessage(topic, payload);
   }

   // Helper to create a test fragment
   static std::vector<uint8_t> createFragment(uint32_t msgId, uint16_t fragNum, uint16_t totalFrags,
                                               const std::string& topic, const std::string& payload)
   {
      std::vector<uint8_t> buffer;
      FragmentHeader header{};
      header.messageId = msgId;
      header.fragmentNum = fragNum;
      header.totalFragments = totalFrags;
      header.topicLen = (fragNum == 0) ? static_cast<uint16_t>(topic.size()) : 0;
      header.reserved = 0;

      buffer.resize(sizeof(FragmentHeader) + (fragNum == 0 ? topic.size() : 0) + payload.size());
      std::memcpy(buffer.data(), &header, sizeof(header));

      size_t offset = sizeof(FragmentHeader);
      if (fragNum == 0 && !topic.empty())
      {
         std::memcpy(buffer.data() + offset, topic.data(), topic.size());
         offset += topic.size();
      }
      std::memcpy(buffer.data() + offset, payload.data(), payload.size());

      return buffer;
   }

   // Helper to add a partial message directly for testing cleanup
   static void addPartialMessage(HighBandwidthSubscriber& sub, uint32_t msgId,
                                  std::chrono::steady_clock::time_point timestamp)
   {
      const std::lock_guard<std::mutex> lock(sub._reassemblyMutex);
      PartialMessage pm;
      pm.totalFragments = 2;
      pm.firstFragmentTime = timestamp;
      sub._partialMessages[msgId] = pm;
   }

   uint16_t _testPort;
   std::string _testMulticastAddr;
};

TEST_F(HighBandwidthSubscriberTest, Constructor_WithDefaultParams_CreatesInstance)
{
   // Arrange & Act
   const HighBandwidthSubscriber subscriber("test_namespace");

   // Assert
   EXPECT_EQ(subscriber.name(), "test_namespace");
   EXPECT_EQ(subscriber.port(), 5670);
}

TEST_F(HighBandwidthSubscriberTest, Constructor_WithCustomParams_CreatesInstance)
{
   // Arrange & Act
   const HighBandwidthSubscriber subscriber("custom_ns", _testMulticastAddr, _testPort, 2000);

   // Assert
   EXPECT_EQ(subscriber.name(), "custom_ns");
   EXPECT_EQ(subscriber.port(), _testPort);
}

TEST_F(HighBandwidthSubscriberTest, Subscribe_BeforeStart_Succeeds)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);
   bool handlerCalled = false;

   // Act - Subscribe should succeed before start()
   subscriber.subscribe("test_topic", [&handlerCalled](const std::string&, const std::string&) {
      handlerCalled = true;
   });

   // Assert - No exception thrown means success
   SUCCEED();
}

TEST_F(HighBandwidthSubscriberTest, Name_ReturnsCorrectNamespace)
{
   // Arrange
   const HighBandwidthSubscriber subscriber("my_namespace", _testMulticastAddr, _testPort);

   // Act & Assert
   EXPECT_EQ(subscriber.name(), "my_namespace");
}

TEST_F(HighBandwidthSubscriberTest, Port_ReturnsCorrectPort)
{
   // Arrange
   const uint16_t customPort = 12346;
   const HighBandwidthSubscriber subscriber("test", _testMulticastAddr, customPort);

   // Act & Assert
   EXPECT_EQ(subscriber.port(), customPort);
}

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(HighBandwidthSubscriberTest, Start_InitializesSocketAndRunningState)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);
   ASSERT_FALSE(isRunning(subscriber));
   ASSERT_EQ(getSocket(subscriber), -1);

   // Act
   const bool started = subscriber.start();

   // Assert
   EXPECT_TRUE(started);
   EXPECT_TRUE(isRunning(subscriber));
   EXPECT_GE(getSocket(subscriber), 0);

   // Cleanup
   subscriber.stop();
}

TEST_F(HighBandwidthSubscriberTest, Start_CalledTwice_ReturnsTrueWithoutError)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);

   // Act
   const bool firstStart = subscriber.start();
   const bool secondStart = subscriber.start();

   // Assert
   EXPECT_TRUE(firstStart);
   EXPECT_TRUE(secondStart);

   // Cleanup
   subscriber.stop();
}

TEST_F(HighBandwidthSubscriberTest, Stop_WhenNotStarted_DoesNotCrash)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);

   // Act & Assert - Should not crash or throw
   subscriber.stop();
   EXPECT_FALSE(isRunning(subscriber));
}

TEST_F(HighBandwidthSubscriberTest, Stop_WhenStarted_SetsCorrectState)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);
   subscriber.start();
   ASSERT_TRUE(isRunning(subscriber));

   // Act
   subscriber.stop();

   // Assert
   EXPECT_FALSE(isRunning(subscriber));
}

// =============================================================================
// Subscribe Tests
// =============================================================================

TEST_F(HighBandwidthSubscriberTest, Subscribe_AddsHandlerToMap)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);

   // Act
   subscriber.subscribe("topic1", [](const std::string&, const std::string&) {});
   subscriber.subscribe("topic2", [](const std::string&, const std::string&) {});

   // Assert
   EXPECT_EQ(getHandlerCount(subscriber), 2);
}

TEST_F(HighBandwidthSubscriberTest, Subscribe_AfterStart_Succeeds)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);
   subscriber.subscribe("early_topic", [](const std::string&, const std::string&) {});
   subscriber.start();
   const size_t initialCount = getHandlerCount(subscriber);

   // Act - Subscribe after start should now work
   subscriber.subscribe("late_topic", [](const std::string&, const std::string&) {});

   // Assert - Handler count should increase
   EXPECT_EQ(getHandlerCount(subscriber), initialCount + 1);

   // Cleanup
   subscriber.stop();
}

TEST_F(HighBandwidthSubscriberTest, Subscribe_SameTopicTwice_OverwritesHandler)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);
   int callCount = 0;

   // Act
   subscriber.subscribe("topic", [](const std::string&, const std::string&) {});
   subscriber.subscribe("topic", [&callCount](const std::string&, const std::string&) { callCount++; });

   // Assert - Should still have only one handler
   EXPECT_EQ(getHandlerCount(subscriber), 1);
}

// =============================================================================
// DeliverMessage Tests (Private Method)
// =============================================================================

TEST_F(HighBandwidthSubscriberTest, DeliverMessage_WithMatchingHandler_CallsHandler)
{
   // Arrange
   HighBandwidthSubscriber subscriber("ns", _testMulticastAddr, _testPort);
   std::string receivedTopic;
   std::string receivedData;

   subscriber.subscribe("mytopic", [&](const std::string& topic, const std::string& data) {
      receivedTopic = topic;
      receivedData = data;
   });

   // Act
   callDeliverMessage(subscriber, "ns/mytopic", "test_payload");

   // Assert
   EXPECT_EQ(receivedTopic, "ns/mytopic");
   EXPECT_EQ(receivedData, "test_payload");
}

TEST_F(HighBandwidthSubscriberTest, DeliverMessage_WithNoMatchingHandler_DoesNotCrash)
{
   // Arrange
   HighBandwidthSubscriber subscriber("ns", _testMulticastAddr, _testPort);

   // Act & Assert - Should not crash
   callDeliverMessage(subscriber, "ns/unknown_topic", "test_payload");
   SUCCEED();
}

// =============================================================================
// CleanupStaleMessages Tests (Private Method)
// =============================================================================

TEST_F(HighBandwidthSubscriberTest, CleanupStaleMessages_RemovesExpiredMessages)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort, 100); // 100ms timeout

   // Add a message that's already expired (timestamp in the past)
   auto expiredTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(200);
   addPartialMessage(subscriber, 1, expiredTime);

   // Add a message that's still fresh
   auto freshTime = std::chrono::steady_clock::now();
   addPartialMessage(subscriber, 2, freshTime);

   ASSERT_EQ(getPartialMessageCount(subscriber), 2);

   // Act
   callCleanupStaleMessages(subscriber);

   // Assert - Only the fresh message should remain
   EXPECT_EQ(getPartialMessageCount(subscriber), 1);
}

TEST_F(HighBandwidthSubscriberTest, CleanupStaleMessages_WithNoMessages_DoesNotCrash)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);
   ASSERT_EQ(getPartialMessageCount(subscriber), 0);

   // Act & Assert - Should not crash
   callCleanupStaleMessages(subscriber);
   EXPECT_EQ(getPartialMessageCount(subscriber), 0);
}

// =============================================================================
// ProcessFragment Tests (Private Method)
// =============================================================================

TEST_F(HighBandwidthSubscriberTest, ProcessFragment_SingleFragmentMessage_DeliversImmediately)
{
   // Arrange
   HighBandwidthSubscriber subscriber("ns", _testMulticastAddr, _testPort);
   std::string receivedTopic;
   std::string receivedData;

   subscriber.subscribe("sensor", [&](const std::string& topic, const std::string& data) {
      receivedTopic = topic;
      receivedData = data;
   });

   // Create a single-fragment message (totalFragments = 1)
   auto fragment = createFragment(1, 0, 1, "ns/sensor", "sensor_data");

   // Act
   callProcessFragment(subscriber, fragment.data(), fragment.size());

   // Assert
   EXPECT_EQ(receivedTopic, "ns/sensor");
   EXPECT_EQ(receivedData, "sensor_data");
   EXPECT_EQ(getPartialMessageCount(subscriber), 0);  // No partial messages left
}

TEST_F(HighBandwidthSubscriberTest, ProcessFragment_TooSmallPacket_IsIgnored)
{
   // Arrange
   HighBandwidthSubscriber subscriber("test", _testMulticastAddr, _testPort);
   std::vector<uint8_t> tinyPacket(4);  // Smaller than FragmentHeader

   // Act & Assert - Should not crash
   callProcessFragment(subscriber, tinyPacket.data(), tinyPacket.size());
   EXPECT_EQ(getPartialMessageCount(subscriber), 0);
}

TEST_F(HighBandwidthSubscriberTest, ProcessFragment_FirstOfMultiple_AddsToPartialMessages)
{
   // Arrange
   HighBandwidthSubscriber subscriber("ns", _testMulticastAddr, _testPort);
   subscriber.subscribe("topic", [](const std::string&, const std::string&) {});

   // Create first fragment of a 2-fragment message
   auto fragment = createFragment(42, 0, 2, "ns/topic", "part1");

   // Act
   callProcessFragment(subscriber, fragment.data(), fragment.size());

   // Assert - Should be stored as partial
   EXPECT_EQ(getPartialMessageCount(subscriber), 1);
}
