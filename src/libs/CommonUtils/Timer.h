#ifndef COMMONUTILS_TIMER_H
#define COMMONUTILS_TIMER_H

#include <iostream>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>
#include <condition_variable>

namespace CommonUtils
{
/**
 * @class Timer
 * @brief Implements a simple timer that can execute a function after a
 *         specified amount of time, or periodically.  Designed for easy
 *         cancellation and destruction.
 */
class Timer {
public:
    /**
     * @brief Constructor for a cancelable timer.
     */
    Timer();

    /**
     *  Destructor - Cancels any pending behavior
     */
    ~Timer();


    /**
     * @brief Executes a function after a specified amount of time
     * @param func - Function to Execute
     * @param interval - Period of time to wait, in milliseconds.
     */
    void startOneShot(const std::function<void()> &func, unsigned int interval);

    /**
     * @brief Executes a function periodically
     * @param func - Function to execute
     * @param interval - Period between executions, in milliseconds
     */
    void startPeriodic(const std::function<void()> &func, unsigned int interval);

    /**
     * @brief Cancells the timer.
     */
    void stop();

private:
    std::atomic<bool> _isRunning;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _cv;
};

}

#endif // COMMONUTILS_TIMER_H
