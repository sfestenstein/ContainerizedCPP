#ifndef HIGHBANDWIDTHPUBLISHER_H
#define HIGHBANDWIDTHPUBLISHER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <netinet/in.h>

#include <google/protobuf/message.h>

/**
 * @brief Fragment header structure for UDP packet fragmentation.
 * 
 * This 12-byte header is prepended to each UDP packet to enable
 * reassembly of large messages that exceed the MTU.
 */
struct FragmentHeader
{
    uint32_t messageId;      ///< Unique ID for this message (groups fragments together)
    uint16_t fragmentNum;    ///< Fragment number (0-based index)
    uint16_t totalFragments; ///< Total number of fragments in the message
    uint16_t topicLen;       ///< Length of topic string (only meaningful in fragment 0)
    uint16_t reserved;       ///< Padding for alignment
} __attribute__((packed));

/**
 * @brief High-bandwidth publisher using raw UDP multicast.
 * 
 * This class provides fast, fire-and-forget messaging optimized for high-bandwidth,
 * high-frequency data transmission. It uses raw UDP multicast sockets for minimal
 * overhead and automatic fragmentation for large messages.
 * 
 * @warning Message delivery is **unreliable**. Packets may be:
 *          - Lost in transit
 *          - Duplicated
 *          - Received out of order
 *          - Silently dropped under network congestion
 * 
 * @note This class is ideal for scenarios where:
 *       - Low latency is more important than guaranteed delivery
 *       - Data is frequently updated (missing one update is acceptable)
 *       - High message throughput is required
 *       - Examples: sensor data, video frames, telemetry, real-time state updates
 * 
 * @see HighBandwidthSubscriber for the corresponding subscriber class
 */
class HighBandwidthPublisher
{
public:
    /**
     * @brief Construct a high-bandwidth UDP multicast publisher.
     * 
     * @param name Namespace for topic isolation (prefixed to all topics)
     * @param multicastAddr Multicast group address (default: "239.192.1.1")
     * @param port UDP port number (default: 5670)
     * @param mtu Maximum transmission unit in bytes (default: 1400 to leave room for IP/UDP headers)
     * @param interfaceAddr Local IP address of the network interface to send on
     *        (default: "" uses INADDR_ANY, letting the OS choose)
     */
    HighBandwidthPublisher(const std::string &name,
                           const std::string &multicastAddr = "239.192.1.1",
                           uint16_t port = 5670,
                           size_t mtu = 1400,
                           const std::string &interfaceAddr = "");

    /**
     * @brief Destructor - closes the UDP socket.
     */
    ~HighBandwidthPublisher();

    /**
     * @brief Publish a protobuf message to the specified topic.
     * 
     * Large messages exceeding the MTU are automatically fragmented into
     * multiple UDP packets. Each packet contains a FragmentHeader for
     * reassembly by the subscriber.
     * 
     * @param topic The topic name (will be prefixed with namespace)
     * @param message The protobuf message to publish
     * @return true if all fragments were sent successfully
     * @return false if serialization or sending failed
     * 
     * @warning This is a fire-and-forget operation. A return value of true
     *          only indicates the packets were sent to the network stack,
     *          not that they were received by any subscriber.
     */
    bool publish(const std::string &topic, const google::protobuf::Message &message);

    /**
     * @brief Get the namespace name.
     * @return The namespace string used for topic prefixing
     */
    [[nodiscard]]const std::string &name() const { return _name; }

    /**
     * @brief Get the UDP port number.
     * @return The port number this publisher sends to
     */
    [[nodiscard]] uint16_t port() const { return _port; }

private:
    std::string _name;                          ///< Namespace for topic isolation
    uint16_t _port;                             ///< UDP port number
    size_t _mtu;                                ///< Maximum transmission unit
    size_t _maxPayloadPerFragment;              ///< Max payload bytes per fragment
    int _socket{-1};                                ///< UDP socket file descriptor
    struct sockaddr_in _multicastAddr;          ///< Multicast destination address
    std::atomic<uint32_t> _messageIdCounter{0}; ///< Counter for unique message IDs
    std::atomic<bool> _running{false};          ///< Running state flag
};

#endif // HIGHBANDWIDTHPUBLISHER_H
