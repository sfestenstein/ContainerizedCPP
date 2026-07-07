#ifndef ZYRESUBSCRIBER_H
#define ZYRESUBSCRIBER_H

#include "ZyreNode.h"

#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

class ZyreSubscriber : public ZyreNode 
{
public:
    // Callback type: receives the raw message data as a string
    using MessageHandler = std::function<void(const std::string &topic, const std::string &data)>;

    /**
     * @brief Construct a Zyre subscriber.
     * @param name Namespace name for topic isolation
        * @param interfaceAddr Network interface to use (interface name or local IP).
        *        (default: "" lets Zyre auto-detect the interface)
     */
    explicit ZyreSubscriber(const std::string &name,
                            const std::string &interfaceAddr = "");
    ~ZyreSubscriber();

    // Subscribe to a topic with a handler callback
    // Can be called at any time while the subscriber is running
    void subscribe(const std::string &topic, MessageHandler handler);

private:
    void receiveLoop();

    std::unordered_map<std::string, MessageHandler> _handlers;
    std::mutex _handlersMutex;
    std::thread _receiveThread;
};

#endif // ZYRESUBSCRIBER_H