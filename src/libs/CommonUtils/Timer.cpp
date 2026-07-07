#include "Timer.h"

namespace CommonUtils
{

Timer::Timer() : _isRunning(false)
{

}

void Timer::startOneShot(const std::function<void ()> &func, unsigned int interval)
{
    if (_isRunning)
    {
        stop();
    }
    _isRunning = true;
    _thread = std::thread([this, func, interval]()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_cv.wait_for(lock, std::chrono::milliseconds(interval), [this]() { return !_isRunning; }))
        {
            return;
        }
        if (_isRunning)
        {
            func();
        }
        _isRunning = false;
    });
}

void Timer::startPeriodic(const std::function<void ()> &func, unsigned int interval)
{
    if (_isRunning)
    {
        stop();
    }
    _isRunning = true;
    _thread = std::thread([this, func, interval]()
    {
        while (_isRunning)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_cv.wait_for(lock, std::chrono::milliseconds(interval), [this]() { return !_isRunning; }))
            {
                return;
            }
            if (_isRunning)
            {
                func();
            }
        }
    });
}

void Timer::stop()
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

Timer::~Timer()
{
    stop();
}

}
