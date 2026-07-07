/**
 * @file TestMain.cpp
 * @brief Custom Google Test main for PubSubTests.
 *
 * Initializes the GeneralLogger before running tests.
 */

#include <gtest/gtest.h>
#include "GeneralLogger.h"

int main(int argc, char** argv)
{
   // Initialize the GeneralLogger for test output
   CommonUtils::GeneralLogger logger;
   logger.init("PubSubTests");

   ::testing::InitGoogleTest(&argc, argv);

   return RUN_ALL_TESTS();
}
