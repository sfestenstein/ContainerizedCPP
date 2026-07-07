/**
 * @file HighBandwidthPublisherUt.cpp
 * @brief Unit tests for HighBandwidthPublisher class.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "HighBandwidthPublisher.h"

class HighBandwidthPublisherTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      // Use a non-default port to avoid conflicts with other tests
      _testPort = 15670;
      _testMulticastAddr = "239.192.100.1";
   }

   void TearDown() override
   {
   }

   uint16_t _testPort;
   std::string _testMulticastAddr;
};

TEST_F(HighBandwidthPublisherTest, Constructor_WithDefaultParams_CreatesInstance)
{
   // Arrange & Act
   const HighBandwidthPublisher publisher("test_namespace");

   // Assert
   EXPECT_EQ(publisher.name(), "test_namespace");
   EXPECT_EQ(publisher.port(), 5670);
}

TEST_F(HighBandwidthPublisherTest, Constructor_WithCustomParams_CreatesInstance)
{
   // Arrange & Act
   const HighBandwidthPublisher publisher("custom_ns", _testMulticastAddr, _testPort, 1200);

   // Assert
   EXPECT_EQ(publisher.name(), "custom_ns");
   EXPECT_EQ(publisher.port(), _testPort);
}

TEST_F(HighBandwidthPublisherTest, Name_ReturnsCorrectNamespace)
{
   // Arrange
   const HighBandwidthPublisher publisher("my_namespace", _testMulticastAddr, _testPort);

   // Act & Assert
   EXPECT_EQ(publisher.name(), "my_namespace");
}

TEST_F(HighBandwidthPublisherTest, Port_ReturnsCorrectPort)
{
   // Arrange
   const uint16_t customPort = 12345;
   const HighBandwidthPublisher publisher("test", _testMulticastAddr, customPort);

   // Act & Assert
   EXPECT_EQ(publisher.port(), customPort);
}
