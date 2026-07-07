#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <functional>
#include "SnoozableTimer.h"

class SnoozableTimerTest : public ::testing::Test {
protected:
    void SetUp() override {
        snExecutionCount = 0;
    }

    static int snExecutionCount;
    std::function<void()> _incrementCount = [&]() { snExecutionCount++; };
};

int SnoozableTimerTest::snExecutionCount = 0;

// Test basic execution timing
TEST_F(SnoozableTimerTest, ExecutesAtSpecifiedTime) {
    auto start = std::chrono::high_resolution_clock::now();
    auto execTime = start + std::chrono::milliseconds(500);

    SnoozableTimer executor(_incrementCount, 500);
    executor.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    executor.stop();

    EXPECT_EQ(snExecutionCount, 1);
    auto now = std::chrono::high_resolution_clock::now();
    EXPECT_GE(now, execTime);
}

// Test adding time before execution
TEST_F(SnoozableTimerTest, AddTimeBeforeExecution) {

    SnoozableTimer executor(_incrementCount, 500);
    executor.start();

    executor.updateSnoozePeriod(500);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    EXPECT_EQ(snExecutionCount, 0); // Shouldn't execute yet

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    executor.stop();

    EXPECT_EQ(snExecutionCount, 1);
}

// Test immediate execution (edge case: time in past)
TEST_F(SnoozableTimerTest, ExecutesImmediatelyForPastTime) {

    SnoozableTimer executor(_incrementCount, -500);
    executor.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    executor.stop();

    EXPECT_EQ(snExecutionCount, 1);
}

// Test multiple starts don't create multiple threads
TEST_F(SnoozableTimerTest, MultipleStartsAreIdempotent) {

    SnoozableTimer executor(_incrementCount, 500);
    executor.start();
    executor.start(); // Second start should do nothing

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    executor.stop();

    EXPECT_EQ(snExecutionCount, 1); // Should only execute once
}

// Test stop before execution
TEST_F(SnoozableTimerTest, StopBeforeExecution) {

    SnoozableTimer executor(_incrementCount, 1000);
    executor.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    executor.stop();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(snExecutionCount, 0);
}

// Test adding negative time
TEST_F(SnoozableTimerTest, AddNegativeTime) {

    SnoozableTimer executor(_incrementCount, 1000);
    executor.start();

    executor.updateSnoozePeriod(-500);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    EXPECT_EQ(snExecutionCount, 1);
    executor.stop();
}

// Test destructor cleanup
TEST_F(SnoozableTimerTest, DestructorCleansUp) {
    {
        SnoozableTimer executor(_incrementCount, 1000);
        executor.start();
    } // Destructor called here

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(snExecutionCount, 0); // Shouldn't execute after destruction
}

// Test zero duration
TEST_F(SnoozableTimerTest, ZeroDuration) {
    SnoozableTimer executor(_incrementCount, 0);
    executor.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(snExecutionCount, 1);
    executor.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(snExecutionCount, 1);
}

// Test basic snooze
TEST_F(SnoozableTimerTest, SingleSnooze) {
    SnoozableTimer executor(_incrementCount, 100);
    executor.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    executor.snooze();
    EXPECT_EQ(snExecutionCount, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    EXPECT_EQ(snExecutionCount, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    EXPECT_EQ(snExecutionCount, 1);
    executor.stop();
    EXPECT_EQ(snExecutionCount, 1);
}

// test Multple snoozes
TEST_F(SnoozableTimerTest, MultiSnooze) {
    SnoozableTimer executor(_incrementCount, 100);
    executor.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    EXPECT_EQ(snExecutionCount, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    EXPECT_EQ(snExecutionCount, 1);
    executor.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    EXPECT_EQ(snExecutionCount, 1);
}

// test Multple snoozes
TEST_F(SnoozableTimerTest, SnoozeTimeoutSnoozeAgain) {
    SnoozableTimer executor(_incrementCount, 100);
    executor.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(snExecutionCount, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    EXPECT_EQ(snExecutionCount, 1);

    executor.snooze();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(snExecutionCount, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_EQ(snExecutionCount, 2);
    executor.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    EXPECT_EQ(snExecutionCount, 2);
}
