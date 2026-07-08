/**
 * @file TestMain.cpp
 * @brief Custom Google Test main for FastDDSTests.
 *
 * Initializes the GeneralLogger before running tests.
 */

#include <gtest/gtest.h>
#include "CommonUtils/GeneralLogger.h"

int main(int argc, char** argv)
{
   CommonUtils::GeneralLogger logger;
   logger.init("FastDDSTests");

   ::testing::InitGoogleTest(&argc, argv);

   return RUN_ALL_TESTS();
}
