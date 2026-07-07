#include <gtest/gtest.h>
#include "Timer.h"
#include <atomic>
#include <thread>

// Test for one-time execution
TEST(TimerTest, OneTimeExecution) 
{
    CommonUtils::Timer timer;
    std::atomic<bool> flag{false};

    timer.startOneShot([&flag]()
    {
        flag = true;
    }, 100); // Execute after 100 milliseconds

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_TRUE(flag);
}

// Test for periodic execution
TEST(TimerTest, PeriodicExecution) 
{
    CommonUtils::Timer timer;
    std::atomic<int> counter{0};

    timer.startPeriodic([&counter]() 
    {
        ++counter;
    }, 100); // Execute every 100 milliseconds

    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    timer.stop();
    EXPECT_GE(counter, 3); // At least 3 executions expected
}

// Test for cancellation
TEST(TimerTest, Cancel)
{
    CommonUtils::Timer timer;
    std::atomic<bool> flag{false};

    timer.startOneShot([&flag]()
    {
        flag = true;
    }, 500); // Execute after 500 milliseconds

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_FALSE(flag);
}
