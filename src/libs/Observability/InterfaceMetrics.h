#ifndef OBSERVABILITY_INTERFACEMETRICS_H_
#define OBSERVABILITY_INTERFACEMETRICS_H_

// OpenTelemetry headers
#include <opentelemetry/sdk/metrics/meter_provider.h>

// Project headers
#include "Observability/MetricsConfig.h"

// System headers
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace Observability
{

/**
 * @brief String constants describing the transport kind being measured.
 */
constexpr char DDS_INTERFACE[] = "DDS";
constexpr char GRPC_INTERFACE[] = "GRPC";
constexpr char ZMQ_INTERFACE[] = "ZMQ";

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
    * @param messageCount   Number of messages represented by this event
    * @param bytes          Size of the message in bytes
    * @param interfaceName  Human-readable name of the interface instance (e.g. "RadarTrackPub")
    * @param type           Transport kind (DDS, GRPC, ZMQ)
    * @param userAttribute  Optional low-cardinality caller-controlled attribute
    */
   virtual void recordSent(uint64_t messageCount, uint64_t bytes,
                           const char *interfaceName,
                           const char *type,
                           const char *userAttribute = "") = 0;

   /**
    * @brief Record one message received on an interface.
    *
    * @param messageCount   Number of messages represented by this event
    * @param bytes          Size of the message in bytes
    * @param interfaceName  Human-readable name of the interface instance (e.g. "WorkstationTrackSub")
    * @param type           Transport kind (DDS, GRPC, ZMQ)
    * @param userAttribute  Optional low-cardinality caller-controlled attribute
    */
   virtual void recordReceived(uint64_t messageCount, uint64_t bytes,
                               const char *interfaceName,
                               const char *type,
                               const char *userAttribute = "") = 0;
};

/**
 * @brief Build the OTel metrics SDK pipeline and register it globally.
 *
 * Builds a metrics exporter from config (OTLP/gRPC, OTLP/HTTP, or console),
 * wraps it in a periodic reader (exporting every config.aggregationPeriod()),
 * and registers the resulting
 * MeterProvider globally. Call once at process startup. Returns the
 * concrete SDK provider so main() can ForceFlush() it on exit.
 *
 * @param config YAML-backed metrics runtime configuration.
 */
std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> init(const MetricsConfig &config);

/**
 * @brief Process-wide accessor for the recording interface.
 *
 * Valid after init() has been called; recording before init() records
 * against OTel's no-op default MeterProvider.
 */
IInterfaceMetrics &metrics();

} // namespace Observability

#endif // OBSERVABILITY_INTERFACEMETRICS_H_
