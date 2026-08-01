#ifndef OBSERVABILITY_EXPORTERPROTOCOL_H_
#define OBSERVABILITY_EXPORTERPROTOCOL_H_

namespace Observability
{

/**
 * @brief Selects which transport a metrics pipeline's exporter uses.
 *
 * A config value rather than a compile-time choice -- see
 * MetricsExporterFactory.h and DESIGN.md's OCP goal.
 */
enum class ExporterProtocol
{
   Grpc,    ///< OTLP over gRPC (default) -- talks to an otel-collector on its gRPC port.
   Http,    ///< OTLP over HTTP -- same collector, different transport.
   Console, ///< Prints metrics to stdout; no collector required.
};

} // namespace Observability

#endif // OBSERVABILITY_EXPORTERPROTOCOL_H_
