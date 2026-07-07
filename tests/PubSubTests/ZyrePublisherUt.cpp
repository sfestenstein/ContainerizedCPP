/**
 * @file ZyrePublisherUt.cpp
 * @brief Unit tests for ZyrePublisher class.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ZyrePublisher.h"

class ZyrePublisherTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
   }

   void TearDown() override
   {
   }
};

TEST_F(ZyrePublisherTest, Constructor_WithValidName_CreatesPublisher)
{
   // Arrange & Act
   const ZyrePublisher publisher("test_publisher");

   // Assert
   EXPECT_EQ(publisher.name(), "test_publisher");
}

TEST_F(ZyrePublisherTest, Name_ReturnsCorrectName)
{
   // Arrange
   const ZyrePublisher publisher("my_publisher");

   // Act & Assert
   EXPECT_EQ(publisher.name(), "my_publisher");
}

TEST_F(ZyrePublisherTest, Constructor_InheritsFromZyreNode)
{
   // Arrange & Act
   ZyrePublisher publisher("test");

   // Assert - ZyrePublisher should be a ZyreNode
   ZyreNode* nodePtr = &publisher;
   EXPECT_NE(nodePtr, nullptr);
   EXPECT_EQ(nodePtr->name(), "test");
}
