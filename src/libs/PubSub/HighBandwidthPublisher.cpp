#include "HighBandwidthPublisher.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "GeneralLogger.h"
HighBandwidthPublisher::HighBandwidthPublisher(const std::string &name,
                                               const std::string &multicastAddr,
                                               uint16_t port,
                                               size_t mtu,
                                               const std::string &interfaceAddr) :
    _name(name),
    _port(port),
    _mtu(mtu),
    _maxPayloadPerFragment(mtu - sizeof(FragmentHeader))
{
    // Create UDP socket
    _socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (_socket < 0)
    {
        GPERROR("Failed to create UDP socket: {}", errno);
        return;
    }

    // Set up multicast destination address
    memset(&_multicastAddr, 0, sizeof(_multicastAddr));
    _multicastAddr.sin_family = AF_INET;
    _multicastAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, multicastAddr.c_str(), &_multicastAddr.sin_addr) != 1)
    {
        GPERROR("Invalid multicast address: {}", multicastAddr);
        close(_socket);
        _socket = -1;
        return;
    }

    // Set TTL for multicast packets (1 = local network only)
    unsigned char ttl = 1;
    if (setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
        GPERROR("Failed to set multicast TTL: {}", errno);
    }

    // Enable loopback so we can test on the same machine
    unsigned char loopback = 1;
    if (setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback)) < 0)
    {
        GPERROR("Failed to enable multicast loopback: {}", errno);
    }

    // Set the outgoing interface for multicast packets
    if (!interfaceAddr.empty())
    {
        struct in_addr localInterface;
        if (inet_pton(AF_INET, interfaceAddr.c_str(), &localInterface) != 1)
        {
            GPERROR("Invalid interface address: {}", interfaceAddr);
        }
        else if (setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_IF,
                            &localInterface, sizeof(localInterface)) < 0)
        {
            GPERROR("Failed to set multicast interface to {}: {}", interfaceAddr, errno);
        }
        else
        {
            GPINFO("Multicast outgoing interface set to {}", interfaceAddr);
        }
    }

    GPINFO("HighBandwidthPublisher created with name '{}' publishing to {}:{} (MTU: {}, max payload/fragment: {})",
           name, multicastAddr, port, mtu, _maxPayloadPerFragment);
    _running.store(true);
}

HighBandwidthPublisher::~HighBandwidthPublisher()
{
    _running.store(false);
    if (_socket >= 0)
    {
        close(_socket);
    }
}

bool HighBandwidthPublisher::publish(const std::string &topic, 
                                      const google::protobuf::Message &message)
{
    if (_socket < 0 || !_running.load())
    {
        return false;
    }

    // Serialize the protobuf message
    std::string serialized;
    if (!message.SerializeToString(&serialized))
    {
        GPERROR("Failed to serialize protobuf message");
        return false;
    }

    // Create namespaced topic
    std::string namespacedTopic = _name + "/" + topic;

    // Calculate total data size: topic (in first fragment) + serialized message
    // First fragment: [header][topic][payload_start]
    // Other fragments: [header][payload_continuation]
    const size_t topicSize = namespacedTopic.size();
    const size_t totalPayloadSize = serialized.size();
    
    // Calculate number of fragments needed
    // First fragment has less payload space due to topic
    const size_t firstFragPayloadSpace = _maxPayloadPerFragment - topicSize;
    size_t numFragmentsCalc = 1;
    
    if (totalPayloadSize > firstFragPayloadSpace)
    {
        const size_t remaining = totalPayloadSize - firstFragPayloadSpace;
        numFragmentsCalc += (remaining + _maxPayloadPerFragment - 1) / _maxPayloadPerFragment;
    }

    if (numFragmentsCalc > 65535)
    {
        GPERROR("Message too large: would require {} fragments", numFragmentsCalc);
        return false;
    }

    const auto numFragments = static_cast<std::uint16_t>(numFragmentsCalc);

    // Get unique message ID
    const uint32_t messageId = _messageIdCounter.fetch_add(1);

    // Send fragments
    std::vector<uint8_t> packet(_mtu);
    size_t payloadOffset = 0;

    for (std::uint16_t fragNum = 0; fragNum < numFragments; ++fragNum)
    {
        auto* header = reinterpret_cast<FragmentHeader*>(packet.data());
        header->messageId = messageId;
        header->fragmentNum = fragNum;
        header->totalFragments = numFragments;
        header->topicLen = (fragNum == 0) ? static_cast<uint16_t>(topicSize) : 0;
        header->reserved = 0;

        size_t packetDataOffset = sizeof(FragmentHeader);
        size_t bytesToSend = 0;

        if (fragNum == 0)
        {
            // First fragment: include topic
            memcpy(packet.data() + packetDataOffset, namespacedTopic.data(), topicSize);
            packetDataOffset += topicSize;
            
            // Add as much payload as fits
            const size_t payloadInFirstFrag = std::min(totalPayloadSize, firstFragPayloadSpace);
            memcpy(packet.data() + packetDataOffset, serialized.data(), payloadInFirstFrag);
            bytesToSend = sizeof(FragmentHeader) + topicSize + payloadInFirstFrag;
            payloadOffset = payloadInFirstFrag;
        }
        else
        {
            // Subsequent fragments: only payload
            const size_t remainingPayload = totalPayloadSize - payloadOffset;
            const size_t payloadInThisFrag = std::min(remainingPayload, _maxPayloadPerFragment);
            memcpy(packet.data() + packetDataOffset, serialized.data() + payloadOffset, payloadInThisFrag);
            bytesToSend = sizeof(FragmentHeader) + payloadInThisFrag;
            payloadOffset += payloadInThisFrag;
        }

        // Send the packet
        const ssize_t sent = sendto(_socket, packet.data(), bytesToSend, 0,
                                    reinterpret_cast<struct sockaddr*>(&_multicastAddr),
                                    sizeof(_multicastAddr));
        if (sent < 0)
        {
            GPERROR("Failed to send fragment {}: {}", fragNum, errno);
            return false;
        }
    }

    return true;
}
