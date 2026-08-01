#include "Observability/MetricsExporterFactory.h"

#include <opentelemetry/exporters/ostream/metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>

namespace Observability
{

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
createMetricExporter(ExporterProtocol protocol)
{
   namespace otlp = opentelemetry::exporter::otlp;
   namespace metrics_exporter = opentelemetry::exporter::metrics;

   switch (protocol)
   {
   case ExporterProtocol::Http:
   {
      otlp::OtlpHttpMetricExporterOptions options;
      return otlp::OtlpHttpMetricExporterFactory::Create(options);
   }
   case ExporterProtocol::Console:
      return metrics_exporter::OStreamMetricExporterFactory::Create();
   case ExporterProtocol::Grpc:
   default:
   {
      otlp::OtlpGrpcMetricExporterOptions options;
      return otlp::OtlpGrpcMetricExporterFactory::Create(options);
   }
   }
}

} // namespace Observability
