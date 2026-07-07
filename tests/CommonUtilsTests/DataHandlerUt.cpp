#include "DataHandler.h"
#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

TEST(DataHandlerTest, SignalDataNotifiesListeners) 
{
    CommonUtils::DataHandler<int> handler;
    std::atomic<int> count{0};
    std::mutex mtx;
    std::condition_variable cv;

    auto listener1 = [&](const int& data) {
        EXPECT_EQ(data, 42);
        count.fetch_add(1);
        cv.notify_one();
    };

    auto listener2 = [&](const int& data) {
        EXPECT_EQ(data, 42);
        count.fetch_add(1);
        cv.notify_one();
    };

    handler.registerListener(listener1);
    handler.registerListener(listener2);

    handler.signalData(42);

    // Wait up to 500ms for both listeners to be called
    std::unique_lock<std::mutex> lk(mtx);
    const bool ok = cv.wait_for(lk, std::chrono::milliseconds(500), [&]{ return count.load() >= 2; });
    EXPECT_TRUE(ok);
}

TEST(DataHandlerTest, ExpiredListenersAreRemoved) 
{
    CommonUtils::DataHandler<int> handler;

    // Use atomic counters and cv to observe listener calls
    std::atomic<int> count{0};
    std::mutex mtx;
    std::condition_variable cv;

    auto listener1 = [&](const int& data) {
        EXPECT_EQ(data, 42);
        count.fetch_add(1);
        cv.notify_one();
    };

    auto regId1 = handler.registerListener(listener1);

    auto listener2 = [&](const int& data) {
        EXPECT_EQ(data, 42);
        count.fetch_add(1);
        cv.notify_one();
    };

    auto regId2 = handler.registerListener(listener2);

    handler.signalData(42);

    // Wait up to 500ms for both listeners to be called
    std::unique_lock<std::mutex> lk(mtx);
    const bool ok = cv.wait_for(lk, std::chrono::milliseconds(500), [&]{ return count.load() >= 2; });
    EXPECT_TRUE(ok);

    // There should be two listeners registered
    EXPECT_EQ(handler.watermarkInfo().first, 2U);

    handler.unregisterListener(regId1);
    EXPECT_EQ(handler.watermarkInfo().first, 1U);

    handler.unregisterListener(regId2);
    EXPECT_EQ(handler.watermarkInfo().first, 0U);

}