#ifndef OBSERVABILITY_INTERFACEMETRICS_H_
#define OBSERVABILITY_INTERFACEMETRICS_H_

// OpenTelemetry headers
#include <opentelemetry/sdk/metrics/meter_provider.h>

// System headers
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Observability
{

/**
 * @brief The kind of transport an interface being measured is using.
 */
enum class InterfaceType
{
   DDS,
   GRPC,
   ZMQ
};

/**
 * @brief Options controlling the OTel SDK pipeline built by init().
 */
struct MetricsOptions
{
   std::string serviceName;
   std::chrono::milliseconds aggregationPeriod{5000};
};

/**
 * @brief Abstract recording surface for interface traffic metrics.
 *
 * Transport libraries (DDS, gRPC, ZMQ, ...) depend only on this interface,
 * never on OpenTelemetry directly -- keeps them free to swap or mock the
 * metrics backend without touching call sites.
 */
class IInterfaceMetrics
{
public:
   virtual ~IInterfaceMetrics() = default;

   /**
    * @brief Record one message sent on an interface.
    *
    * @param interfaceName Human-readable name of the interface instance (e.g. "RadarTrackPub")
    * @param type           Transport kind (DDS, GRPC, ZMQ)
    * @param topic          Topic/channel name the message was sent on
    * @param bytes          Size of the message in bytes
    */
   virtual void recordSent(std::string_view interfaceName, InterfaceType type,
                            std::string_view topic, uint64_t bytes) = 0;

   /**
    * @brief Record one message received on an interface.
    *
    * @param interfaceName Human-readable name of the interface instance (e.g. "WorkstationTrackSub")
    * @param type           Transport kind (DDS, GRPC, ZMQ)
    * @param topic          Topic/channel name the message was received on
    * @param bytes          Size of the message in bytes
    */
   virtual void recordReceived(std::string_view interfaceName, InterfaceType type,
                                std::string_view topic, uint64_t bytes) = 0;
};

/**
 * @brief Build the OTel metrics SDK pipeline and register it globally.
 *
 * Builds an OTLP/gRPC exporter, wraps it in a periodic reader (exporting
 * every options.aggregationPeriod), and registers the resulting
 * MeterProvider globally. Call once at process startup. Returns the
 * concrete SDK provider so main() can ForceFlush() it on exit.
 *
 * @param options Service name (used as the "service.name" resource
 *                attribute) and export aggregation period.
 */
std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> init(const MetricsOptions &options);

/**
 * @brief Process-wide accessor for the recording interface.
 *
 * Valid after init() has been called; recording before init() records
 * against OTel's no-op default MeterProvider.
 */
IInterfaceMetrics &metrics();

} // namespace Observability

#endif // OBSERVABILITY_INTERFACEMETRICS_H_
