#ifndef HIGHBANDWIDTHSUBSCRIBER_H
#define HIGHBANDWIDTHSUBSCRIBER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declaration - FragmentHeader is defined in HighBandwidthPublisher.h
struct FragmentHeader;

/**
 * @brief Structure to hold partially reassembled messages.
 * 
 * Used internally to buffer incoming fragments until a complete
 * message can be reconstructed and delivered.
 */
struct PartialMessage
{
    std::string                  topic;                      ///< Topic extracted from fragment 0
    std::vector<std::string>     fragments;                  ///< Fragment payloads indexed by fragment number
    std::unordered_set<uint16_t> receivedFragments;          ///< Set of received fragment numbers
    uint16_t                     totalFragments;             ///< Expected total number of fragments
    std::chrono::steady_clock::time_point firstFragmentTime; ///< Timestamp of first fragment arrival
};

/**
 * @brief High-bandwidth subscriber using raw UDP multicast.
 * 
 * This class provides fast message reception optimized for high-bandwidth,
 * high-frequency data. It uses raw UDP multicast sockets and handles
 * automatic reassembly of fragmented messages.
 * 
 * @warning Message delivery is **unreliable**. Messages may be:
 *          - Lost entirely if any fragment is dropped
 *          - Incomplete if fragments arrive after the reassembly timeout
 *          - Out of order (though reassembly handles this)
 * 
 * @note This class is ideal for scenarios where:
 *       - Low latency is more important than guaranteed delivery
 *       - Data is frequently updated (missing one update is acceptable)
 *       - High message throughput is required
 *       - Examples: sensor data, video frames, telemetry, real-time state updates
 * 
 * @see HighBandwidthPublisher for the corresponding publisher class
 */
// Forward declaration for friend test class
class HighBandwidthSubscriberTest;

class HighBandwidthSubscriber
{
    friend class HighBandwidthSubscriberTest;

public:
    /**
     * @brief Callback type for message handlers.
     * 
     * @param topic The full namespaced topic string
     * @param data The reassembled message payload (serialized protobuf)
     */
    using MessageHandler = std::function<void(const std::string &topic, const std::string &data)>;

    /**
     * @brief Construct a high-bandwidth UDP multicast subscriber.
     * 
     * @param name Namespace for topic isolation (must match publisher's namespace)
     * @param multicastAddr Multicast group address to join (default: "239.192.1.1")
     * @param port UDP port number (default: 5670)
     * @param reassemblyTimeoutMs Timeout in milliseconds to wait for all fragments
     *        before discarding incomplete messages (default: 1000ms)
     * @param interfaceAddr Local IP address of the network interface to receive on
     *        (default: "" uses INADDR_ANY, letting the OS choose)
     */
    HighBandwidthSubscriber(const std::string &name,
                            const std::string &multicastAddr = "239.192.1.1",
                            std::uint16_t port = 5670,
                            int reassemblyTimeoutMs = 1000,
                            const std::string &interfaceAddr = "");

    /**
     * @brief Destructor - stops receiving and closes the socket.
     */
    ~HighBandwidthSubscriber();

    /**
     * @brief Subscribe to a topic with a callback handler.
     * 
     * @param topic The topic name to subscribe to (without namespace prefix)
     * @param handler Callback function invoked when a complete message is received
     * 
     * @note Thread-safe. Can be called before or after start().
     * 
     * @warning The handler is called from the receive thread. Keep handlers
     *          fast to avoid dropping incoming packets.
     */
    void subscribe(const std::string &topic, MessageHandler handler);

    /**
     * @brief Start receiving messages.
     * 
     * Creates the UDP socket, joins the multicast group, and starts the
     * background receive thread.
     * 
     * @return true if started successfully
     * @return false if socket creation or multicast join failed
     */
    bool start();

    /**
     * @brief Stop receiving messages.
     * 
     * Signals the receive thread to stop and waits for it to exit.
     * Incomplete messages in the reassembly buffer are discarded.
     */
    void stop();

    /**
     * @brief Get the namespace name.
     * @return The namespace string used for topic filtering
     */
    const std::string &name() const { return _name; }
    /**
     * @brief Get the UDP port number.
     * @return The UDP port number used for multicast
     */
    [[nodiscard]] std::uint16_t port() const { return _port; }
private:
    /**
     * @brief Background thread function for receiving packets.
     */
    void receiveLoop();

    /**
     * @brief Clean up incomplete messages that have timed out.
     */
    void cleanupStaleMessages();

    /**
     * @brief Process a received fragment and reassemble if complete.
     * @param data Pointer to raw packet data
     * @param len Length of packet data
     */
    void processFragment(const uint8_t *data, size_t len);

    /**
     * @brief Deliver a complete message to the appropriate handler.
     * @param topic The full namespaced topic
     * @param payload The reassembled message payload
     */
    void deliverMessage(const std::string &topic, const std::string &payload);

    std::string _name;              ///< Namespace for topic filtering
    std::string _multicastAddr;     ///< Multicast group address
    std::string _interfaceAddr;     ///< Local interface address for multicast
    uint16_t _port;                 ///< UDP port number
    int _reassemblyTimeoutMs;       ///< Timeout for incomplete messages
    int _socket{-1};                ///< UDP socket file descriptor
    std::atomic<bool> _running{false};    ///< Running state flag
    std::atomic<bool> _shouldStop{false}; ///< Stop request flag

    std::unordered_map<std::string, MessageHandler> _handlers; ///< Topic -> handler map
    std::mutex _handlersMutex;                                  ///< Protects _handlers

    std::unordered_map<uint32_t, PartialMessage> _partialMessages; ///< Reassembly buffer
    std::mutex _reassemblyMutex;                                    ///< Protects reassembly buffer

    std::thread _receiveThread;     ///< Background receive thread
};

#endif // HIGHBANDWIDTHSUBSCRIBER_H
