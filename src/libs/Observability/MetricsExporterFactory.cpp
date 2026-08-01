#include "Observability/MetricsExporterFactory.h"

#include <opentelemetry/exporters/ostream/metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>

namespace Observability
{

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
createMetricExporter(ExporterProtocol protocol, const std::string &endpoint)
{
   namespace otlp = opentelemetry::exporter::otlp;
   namespace metrics_exporter = opentelemetry::exporter::metrics;

   switch (protocol)
   {
   case ExporterProtocol::Http:
   {
      otlp::OtlpHttpMetricExporterOptions options;
      // JSON rather than the default binary protobuf -- lets lightweight
      // receivers (e.g. a demo dashboard) parse the export with a plain
      // JSON library instead of needing protobuf tooling.
      options.content_type = otlp::HttpRequestContentType::kJson;
      if (!endpoint.empty())
      {
         options.url = endpoint;
      }
      return otlp::OtlpHttpMetricExporterFactory::Create(options);
   }
   case ExporterProtocol::Console:
      return metrics_exporter::OStreamMetricExporterFactory::Create();
   case ExporterProtocol::Grpc:
   default:
   {
      otlp::OtlpGrpcMetricExporterOptions options;
      if (!endpoint.empty())
      {
         options.endpoint = endpoint;
      }
      return otlp::OtlpGrpcMetricExporterFactory::Create(options);
   }
   }
}

} // namespace Observability
