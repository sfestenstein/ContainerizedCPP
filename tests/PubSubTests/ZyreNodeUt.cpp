/**
 * @file ZyreNodeUt.cpp
 * @brief Unit tests for ZyreNode class.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ZyreNode.h"

/**
 * @brief Test fixture for ZyreNode tests.
 * 
 * Note: ZyreNode is an abstract base class, so we create a concrete
 * derived class for testing purposes.
 */
class TestableZyreNode : public ZyreNode
{
public:
   explicit TestableZyreNode(const std::string& name)
      : ZyreNode(name)
   {
   }

   // Expose protected members for testing
   [[nodiscard]] bool isRunning() const { return _isRunning.load(); }
   [[nodiscard]] zyre_t* getNode() const { return _node; }
};

class ZyreNodeTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
   }

   void TearDown() override
   {
   }
};

TEST_F(ZyreNodeTest, Constructor_WithValidName_CreatesNode)
{
   // Arrange & Act
   const TestableZyreNode node("test_node");

   // Assert
   EXPECT_EQ(node.name(), "test_node");
}

TEST_F(ZyreNodeTest, Name_ReturnsCorrectName)
{
   // Arrange
   const TestableZyreNode node("my_zyre_node");

   // Act & Assert
   EXPECT_EQ(node.name(), "my_zyre_node");
}

TEST_F(ZyreNodeTest, Constructor_InitializesRunningFlag)
{
   // Arrange & Act
   const TestableZyreNode node("test_node");
   // Assert - Node should be in running state initially
   EXPECT_TRUE(node.isRunning());
}

TEST_F(ZyreNodeTest, Stop_SetsRunningFlagToFalse)
{
   // Arrange
   TestableZyreNode node("test_node");
   ASSERT_TRUE(node.isRunning());

   // Act
   node.stop();

   // Assert
   EXPECT_FALSE(node.isRunning());
}
