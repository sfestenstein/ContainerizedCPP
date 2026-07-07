/**
 * @file TestMain.cpp
 * @brief Google Test main for Vita49_2Tests.
 *
 * No external dependencies — just standard GTest.
 */

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
