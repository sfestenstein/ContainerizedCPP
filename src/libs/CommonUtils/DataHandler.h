#ifndef COMMONUTILS_DATAHANDLER_H
#define COMMONUTILS_DATAHANDLER_H
#include <functional>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include <atomic>
#include <iostream>

namespace CommonUtils
{
/**
 * @class DataHandler
 * @brief Implements a thread safe queue.
 *        * Listeners can register an std::function to listen for new data
 *        * Any thread can signal that new data is available.
 *        * listener functions are executed in a common, but separate thread,
 */
template <typename T>
class DataHandler {
public:
    using Listener = std::function<void(const T&)>;

    /**
     * @brief Constructor
     */
    DataHandler() : _stopFlag(false)
    {
        _workerThread = std::thread(&DataHandler::processData, this);
    }

    /**
     * Cleanly destroys an object of this type!
     */
    ~DataHandler()
    {
        _stopFlag = true;
        _condVar.notify_all();
        // Wait for the worker thread to finish processing
        if (_workerThread.joinable())
        {
            _workerThread.join();
        }

        // Clear all listeners and data queue
        {
            const std::lock_guard<std::mutex> lock(_listenersMutex);
            _listeners.clear();
        }

        while (!_dataQueue.empty())
        {
            _dataQueue.pop();
        }
    }

    /**
     * @brief Indicates new data is available for the listeners to consume.
     * @param data
     */
    void signalData(const T& data)
    {
        if (_stopFlag) return;

        const std::lock_guard<std::mutex> lock(_cvMutex);
        {
            _dataQueue.push(data);
        }
        _condVar.notify_one();
    }

    /**
     * @brief Allows anyone to register a function to be called
     *        when new data is signaled. an ID is returned
     *        which can be used to unregister the listener.
     * @return A registration ID that can be used to unregister the listener.
     *         The ID is guaranteed to be unique for each listener.
     * @param listener
     */
    int registerListener(const Listener &listener)
    {
        if (_stopFlag) return -1; // Prevent registration after stop
        const std::lock_guard<std::mutex> lock(_listenersMutex);
        _nextListenerId++;
        _listeners[_nextListenerId] = listener;
        return _nextListenerId;
    }

    /**
     * @brief Unregisters a listener by its registration ID.
     * @param id The registration ID returned by registerListener.
     * @return true if a listener was removed, false otherwise.
     */
    void unregisterListener(int id)
    {
        if (_stopFlag) return; // Prevent deregistration after stop
        const std::lock_guard<std::mutex> lock(_listenersMutex);
        _listeners.erase(id);
    }

    /**
     * @brief Returns usage statistics of this class
     * @return std pair, first item is the number of listeners,
     *                   second is the number of dataum in the
     *                   queue.
     */
    std::pair <size_t, size_t> watermarkInfo()
    {        
        if (_stopFlag) return {0, 0};

        const std::lock_guard<std::mutex> lock(_listenersMutex);
        return {_listeners.size(), _dataQueue.size()};
    }

private:
    void processData() 
    {
        while (!_stopFlag) 
        {
            T data;
            {
                std::unique_lock<std::mutex> lock(_cvMutex);
                _condVar.wait(lock, [this] { return !_dataQueue.empty() || _stopFlag; });
                if (_stopFlag && _dataQueue.empty()) {
                    return;
                }
                data = _dataQueue.front();
                _dataQueue.pop();
            }
            notifyListeners(data);
        }
    }

    void notifyListeners(const T& data) 
    {
        const std::lock_guard<std::mutex> lock(_listenersMutex);
        for (const auto &listener : _listeners) 
        {
            try
            {
                listener.second(data);
            }
            catch(const std::exception& e)
            {
                std::cerr << "Listener threw an std::exception! " << e.what() << '\n';
            }
            catch(...)
            {
                std::cerr << "Listener threw an unknown exception!\n";
            }
        }
    }

    std::mutex _listenersMutex;
    std::map<int, Listener> _listeners;
    int _nextListenerId = 123;
    std::queue<T> _dataQueue;
    std::mutex _cvMutex;
    std::condition_variable _condVar;
    std::thread _workerThread;
    std::atomic<bool> _stopFlag;
};
}

#endif // COMMONUTILS_DATAHANDLER_H
