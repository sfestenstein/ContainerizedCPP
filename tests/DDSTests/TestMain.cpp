/**
 * @file TestMain.cpp
 * @brief Custom Google Test main for DDSTests.
 *
 * Initializes the GeneralLogger before running tests.
 */

#include <gtest/gtest.h>
#include "GeneralLogger.h"

int main(int argc, char** argv)
{
   CommonUtils::GeneralLogger logger;
   logger.init("DDSTests");

   ::testing::InitGoogleTest(&argc, argv);

   return RUN_ALL_TESTS();
}
