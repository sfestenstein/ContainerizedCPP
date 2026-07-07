#include "HighBandwidthSubscriber.h"
#include "HighBandwidthPublisher.h"  // For FragmentHeader definition

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <utility>

#include <GeneralLogger.h>

HighBandwidthSubscriber::HighBandwidthSubscriber(const std::string &name,
                                                 const std::string &multicastAddr,
                                                 uint16_t port,
                                                 int reassemblyTimeoutMs,
                                                 const std::string &interfaceAddr) :
    _name(name),
    _multicastAddr(multicastAddr),
    _interfaceAddr(interfaceAddr),
    _port(port),
    _reassemblyTimeoutMs(reassemblyTimeoutMs)
{
}

HighBandwidthSubscriber::~HighBandwidthSubscriber()
{
    stop();

    if (_socket >= 0)
    {
        close(_socket);
    }
}

void HighBandwidthSubscriber::subscribe(const std::string &topic, MessageHandler handler)
{
    // Create namespaced topic
    const std::string namespacedTopic = _name + "/" + topic;

    const std::lock_guard<std::mutex> lock(_handlersMutex);
    _handlers[namespacedTopic] = std::move(handler);
}

bool HighBandwidthSubscriber::start()
{
    if (_running.load())
    {
        return true;
    }

    // Create UDP socket
    _socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (_socket < 0)
    {
        GPERROR("Failed to create UDP socket: ", errno);
        return false;
    }

    // Allow multiple sockets to use the same port
    int reuse = 1;
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        GPERROR("Failed to set SO_REUSEADDR: ", errno);
    }

    // Bind to the multicast port
    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(_port);
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(_socket, reinterpret_cast<struct sockaddr*>(&localAddr), sizeof(localAddr)) < 0)
    {
        GPERROR("Failed to bind socket to port {} : {}", _port, errno);
        close(_socket);
        _socket = -1;
        return false;
    }

    // Join the multicast group
    struct ip_mreq mreq;
    if (inet_pton(AF_INET, _multicastAddr.c_str(), &mreq.imr_multiaddr) != 1)
    {
        GPERROR("Invalid multicast address: {}", _multicastAddr);
        close(_socket);
        _socket = -1;
        return false;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    // If a specific interface address was provided, use it for the multicast join
    if (!_interfaceAddr.empty())
    {
        if (inet_pton(AF_INET, _interfaceAddr.c_str(), &mreq.imr_interface) != 1)
        {
            GPERROR("Invalid interface address: {}", _interfaceAddr);
            close(_socket);
            _socket = -1;
            return false;
        }
        GPINFO("Joining multicast group on interface {}", _interfaceAddr);
    }

    if (setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        GPERROR("Failed to join multicast group: {}", errno);
        close(_socket);
        _socket = -1;
        return false;
    }

    GPINFO("HighBandwidthSubscriber joined multicast group {}:{}", _multicastAddr, _port);

    _shouldStop.store(false);
    _running.store(true);

    _receiveThread = std::thread(&HighBandwidthSubscriber::receiveLoop, this);

    return true;
}

void HighBandwidthSubscriber::stop()
{
    if (!_running.load())
    {
        return;
    }

    _shouldStop.store(true);
    _running.store(false);

    if (_receiveThread.joinable())
    {
        _receiveThread.join();
    }
}

void HighBandwidthSubscriber::receiveLoop()
{
    std::vector<uint8_t> buffer(65535);  // Max UDP packet size
    auto lastCleanup = std::chrono::steady_clock::now();

    while (_running.load() && !_shouldStop.load())
    {
        // Poll with timeout to allow checking _running flag
        struct pollfd pfd;
        pfd.fd = _socket;
        pfd.events = POLLIN;
        
        const int ret = poll(&pfd, 1, 100);  // 100ms timeout
        
        if (ret < 0)
        {
            if (errno != EINTR)
            {
                GPERROR("poll() failed: {}", errno);
            }
            continue;
        }

        if (ret == 0)
        {
            // Timeout - clean up stale partial messages
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCleanup).count() > 500)
            {
                cleanupStaleMessages();
                lastCleanup = now;
            }
            continue;
        }

        // Receive packet
        const ssize_t received = recv(_socket, buffer.data(), buffer.size(), 0);
        if (received < 0)
        {
            if (errno != EINTR && errno != EAGAIN)
            {
                GPERROR("recv() failed: {}", errno);
            }
            continue;
        }

        if (std::cmp_greater_equal(received, sizeof(FragmentHeader)))
        {
            processFragment(buffer.data(), static_cast<size_t>(received));
        }
    }
}

void HighBandwidthSubscriber::cleanupStaleMessages()
{
    const std::lock_guard<std::mutex> lock(_reassemblyMutex);
    auto now = std::chrono::steady_clock::now();
    
    auto it = _partialMessages.begin();
    while (it != _partialMessages.end())
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.firstFragmentTime).count();
        
        if (elapsed > _reassemblyTimeoutMs)
        {
            // Discard incomplete message
            it = _partialMessages.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void HighBandwidthSubscriber::processFragment(const uint8_t *data, size_t len)
{
    // Validate minimum packet size
    if (len < sizeof(FragmentHeader))
    {
        return;  // Too small to be a valid fragment
    }

    const auto* header = reinterpret_cast<const FragmentHeader*>(data);
    const uint8_t *payload = data + sizeof(FragmentHeader);
    const size_t payloadLen = len - sizeof(FragmentHeader);

    const std::uint32_t messageId = header->messageId;
    const std::uint16_t fragNum = header->fragmentNum;
    const std::uint16_t totalFrags = header->totalFragments;
    const std::uint16_t topicLen = header->topicLen;

    const std::lock_guard<std::mutex> lock(_reassemblyMutex);
    // Get or create partial message entry
    auto &partial = _partialMessages[messageId];
    
    if (partial.fragments.empty())
    {
        // First fragment for this message ID
        partial.totalFragments = totalFrags;
        partial.fragments.resize(totalFrags);
        partial.firstFragmentTime = std::chrono::steady_clock::now();
    }

    // Check consistency
    if (partial.totalFragments != totalFrags)
    {
        // Inconsistent fragment count - discard
        _partialMessages.erase(messageId);
        return;
    }

    // Check if we already have this fragment
    if (partial.receivedFragments.contains(fragNum))
    {
        return;  // Duplicate
    }

    // Process fragment content
    if (fragNum == 0)
    {
        // First fragment contains topic
        if (topicLen > payloadLen)
        {
            _partialMessages.erase(messageId);
            return;
        }
        partial.topic = std::string(reinterpret_cast<const char*>(payload), topicLen);
        partial.fragments[0] = std::string(reinterpret_cast<const char*>(payload + topicLen), 
                                            payloadLen - topicLen);
    }
    else
    {
        partial.fragments[fragNum] = std::string(reinterpret_cast<const char*>(payload), payloadLen);
    }

    partial.receivedFragments.insert(fragNum);
    // Check if message is complete
    if (partial.receivedFragments.size() == totalFrags)
    {
        // Reassemble payload
        std::string fullPayload;
        for (const auto &frag : partial.fragments)
        {
            fullPayload += frag;
        }

        const std::string topic = partial.topic;
        _partialMessages.erase(messageId);

        // Release lock before calling handler
        _reassemblyMutex.unlock();
        deliverMessage(topic, fullPayload);
        _reassemblyMutex.lock();
    }
}

void HighBandwidthSubscriber::deliverMessage(const std::string &topic, const std::string &payload)
{
    MessageHandler handler;
    {
        const std::lock_guard<std::mutex> lock(_handlersMutex);
        auto it = _handlers.find(topic);
        if (it != _handlers.end())
        {
            handler = it->second;
        }
    }

    if (handler)
    {
        handler(topic, payload);
    }
}
