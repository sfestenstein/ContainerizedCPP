#include "SnoozableTimer.h"
#include <chrono>
#include <utility>

SnoozableTimer::SnoozableTimer(std::function<void ()> ahFuncion, int anSnoozePeriodMs)
    : _function(std::move(ahFuncion))
    , _snoozePeriodMs(anSnoozePeriodMs)
{

}

SnoozableTimer::~SnoozableTimer()
{
    stop();
}

void SnoozableTimer::start()
{
    if (_isRunning) return;

    _executionTime = std::chrono::high_resolution_clock::now() +
                      std::chrono::milliseconds(_snoozePeriodMs);

    _isRunning = true;
    _thread = std::thread([this]()
    {
        std::unique_lock<std::mutex> lock(_mutex);

        while (_isRunning)
        {
            // Wait until execution time or until notified
            if (_cv.wait_until(lock, _executionTime) == std::cv_status::timeout && _isRunning)
            {
                lock.unlock();
                _function();
                lock.lock();
                // This should prevent reexecution until we hit the snooze!
                _executionTime = std::chrono::high_resolution_clock::now() +
                                 std::chrono::hours(87600);
            }
        }
    });
}

void SnoozableTimer::stop()
{
    {
        const std::lock_guard<std::mutex> lock(_mutex);
        _isRunning = false;
    }

    _cv.notify_all();
    if (_thread.joinable())
    {
        _thread.join();
    }
}

void SnoozableTimer::snooze()
{
    const std::lock_guard<std::mutex> lock(_mutex);
    auto lcTimeNow = std::chrono::high_resolution_clock::now() +
            std::chrono::milliseconds(_snoozePeriodMs);
    _executionTime = lcTimeNow;
    _cv.notify_all(); // Notify to re-evaluate the wait condition
}

void SnoozableTimer::updateSnoozePeriod(int anSnoozePeriodMs)
{
    {
        const std::lock_guard<std::mutex> lock(_mutex);
        _snoozePeriodMs = anSnoozePeriodMs;
    }
    snooze();
}
