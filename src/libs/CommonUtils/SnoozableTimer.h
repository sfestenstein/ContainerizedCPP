#ifndef SNOOZABLETIMER_H
#define SNOOZABLETIMER_H

#include <functional>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

/**
 * @class SnoozableTimer
 * @brief This class will execute an std::function after a snooze period
 *        of milliseconds once it is started.  It's key feature is a 'snooze'
 *        method that will increase the time until execution.
 */
class SnoozableTimer
{
public:

    /**
     * @brief Constructor
     * @param func
     * @param anTimeoutMs
     */
    SnoozableTimer(std::function<void()> ahFunciont, int anSnoozePeriodMs );

    /**
     * @brief Destructor
     */
    ~SnoozableTimer();

    /**
     * @brief Starts the timer
     */
    void start();

    /**
     * @brief Stopps the timer
     */
    void stop();

    /**
     * @brief noozes until NOW plus the snooze time
     */
    void snooze();

    /**
     * @brief Updatest he value of the snooze period.
     *       !!! Will execute a snooze of the new duration !!!
     * @param anSnoozePeriodMs
     */
    void updateSnoozePeriod(int anSnoozePeriodMs);

private:
    std::function<void()> _function;
    std::chrono::high_resolution_clock::time_point _executionTime;
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _thread;
    int _snoozePeriodMs;
    bool _isRunning = false;
};


#endif // SNOOZABLETIMER_H
