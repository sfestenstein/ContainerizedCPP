/**
 * @file ZyreSubscriberUt.cpp
 * @brief Unit tests for ZyreSubscriber class.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ZyreSubscriber.h"

class ZyreSubscriberTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
   }

   void TearDown() override
   {
   }
};

TEST_F(ZyreSubscriberTest, Constructor_WithValidName_CreatesSubscriber)
{
   // Arrange & Act
   const ZyreSubscriber subscriber("test_subscriber");

   // Assert
   EXPECT_EQ(subscriber.name(), "test_subscriber");
}

TEST_F(ZyreSubscriberTest, Name_ReturnsCorrectName)
{
   // Arrange
   const ZyreSubscriber subscriber("my_subscriber");

   // Act & Assert
   EXPECT_EQ(subscriber.name(), "my_subscriber");
}

TEST_F(ZyreSubscriberTest, Constructor_InheritsFromZyreNode)
{
   // Arrange & Act
   ZyreSubscriber subscriber("test");
   // Assert - ZyreSubscriber should be a ZyreNode
   ZyreNode* nodePtr = &subscriber;
   EXPECT_NE(nodePtr, nullptr);
   EXPECT_EQ(nodePtr->name(), "test");
}

TEST_F(ZyreSubscriberTest, Subscribe_WithValidTopic_Succeeds)
{
   // Arrange
   ZyreSubscriber subscriber("test");
   bool handlerRegistered = false;

   // Act - Subscribe should not throw
   subscriber.subscribe("test_topic", [&handlerRegistered](const std::string&, const std::string&) {
      handlerRegistered = true;
   });

   // Assert - No exception means success
   SUCCEED();
}

TEST_F(ZyreSubscriberTest, Subscribe_MultipleTopic_Succeeds)
{
   // Arrange
   ZyreSubscriber subscriber("test");

   // Act - Subscribe to multiple topics
   subscriber.subscribe("topic1", [](const std::string&, const std::string&) {});
   subscriber.subscribe("topic2", [](const std::string&, const std::string&) {});
   subscriber.subscribe("topic3", [](const std::string&, const std::string&) {});

   // Assert - No exception means success
   SUCCEED();
}
