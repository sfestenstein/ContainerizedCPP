#ifndef OBSERVABILITY_IINTERFACEMETRICS_H_
#define OBSERVABILITY_IINTERFACEMETRICS_H_

// System headers
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace Observability
{

/**
 * @brief String constants for the transport an interface being measured is
 *        using. Plain strings (rather than an enum) since the value only
 *        ever flows straight through to the OTel "interface.type"
 *        attribute -- there is no enum-specific behavior to gain.
 */
namespace InterfaceType
{
constexpr const char *DDS_INTERFACE = "DDS";
constexpr const char *GRPC_INTERFACE = "GRPC";
constexpr const char *ZMQ_INTERFACE = "ZMQ";
} // namespace InterfaceType

/**
 * @brief Abstract recording surface for interface traffic metrics.
 *
 * Transport libraries (DDS, gRPC, ZMQ, ...) depend only on this interface,
 * never on OpenTelemetry directly. No OTel includes anywhere in this header
 * -- callers get the interface without pulling in the OTel SDK transitively.
 *
 * There is no constructor-injection seam here: callers reach an
 * implementation of this interface via the swappable global registry in
 * MetricsRegistry.h (Observability::metrics()), not via this header alone.
 * See DESIGN.md for why that tradeoff was chosen.
 */
class IInterfaceMetrics
{
public:
   virtual ~IInterfaceMetrics() = default;

   /**
    * @brief Record one message sent on an interface.
    *
    * @param interfaceName Human-readable name of the interface instance (e.g. "RadarTrackPub")
    * @param type           Transport kind, e.g. InterfaceType::DDS_INTERFACE
    * @param topic          Topic/channel name the message was sent on
    * @param bytes          Size of the message in bytes
    */
   virtual void recordSent(const std::string &interfaceName, const char *type,
                            const std::string &topic, uint64_t bytes) = 0;

   /**
    * @brief Record one message received on an interface.
    *
    * @param interfaceName Human-readable name of the interface instance (e.g. "WorkstationTrackSub")
    * @param type           Transport kind, e.g. InterfaceType::DDS_INTERFACE
    * @param topic          Topic/channel name the message was received on
    * @param bytes          Size of the message in bytes
    */
   virtual void recordReceived(const std::string &interfaceName, const char *type,
                                const std::string &topic, uint64_t bytes) = 0;

   /**
    * @brief Record the send-to-receive latency of one message.
    *
    * @param interfaceName Human-readable name of the interface instance
    * @param type           Transport kind, e.g. InterfaceType::DDS_INTERFACE
    * @param topic          Topic/channel name the message was received on
    * @param latency        Elapsed time between the sender's timestamp and receipt
    */
   virtual void recordLatency(const std::string &interfaceName, const char *type,
                               const std::string &topic, std::chrono::nanoseconds latency) = 0;

   /**
    * @brief Register a subscriber instance as a source for the
    *        "interface.subscriber.active" observable gauge.
    *
    * Multiple DDSSubscriber instances come and go at runtime and report into
    * one shared gauge instrument; each hands over a pointer to its own
    * `std::atomic<bool>` running-flag rather than `this`, so the gauge's
    * collection callback never has to dereference a subscriber that may have
    * already been destroyed -- see DESIGN.md's "Push vs. pull instruments".
    *
    * @param interfaceName Human-readable name of the interface instance
    * @param type           Transport kind, e.g. InterfaceType::DDS_INTERFACE
    * @param topic          Topic/channel name being subscribed to
    * @param activeFlag     Pointer to the subscriber's running flag, read
    *                       (never written) on each collection cycle. Must
    *                       outlive the registration -- call
    *                       unregisterActiveProbe() before the pointee is
    *                       destroyed.
    */
   virtual void registerActiveProbe(const std::string &interfaceName, const char *type,
                                     const std::string &topic,
                                     const std::atomic<bool> *activeFlag) = 0;

   /**
    * @brief Remove a probe previously added by registerActiveProbe().
    *
    * @param activeFlag The same pointer passed to registerActiveProbe().
    */
   virtual void unregisterActiveProbe(const std::atomic<bool> *activeFlag) = 0;
};

} // namespace Observability

#endif // OBSERVABILITY_IINTERFACEMETRICS_H_
