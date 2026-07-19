#include "Observability/InterfaceMetrics.h"

#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>
#include <opentelemetry/sdk/resource/resource.h>

#include <map>
#include <string>

namespace Observability
{

namespace
{

std::string_view toString(InterfaceType type)
{
   switch (type)
   {
   case InterfaceType::DDS:
      return "DDS";
   case InterfaceType::GRPC:
      return "GRPC";
   case InterfaceType::ZMQ:
      return "ZMQ";
   }
   return "UNKNOWN";
}

class OtelInterfaceMetrics : public IInterfaceMetrics
{
public:
   void recordSent(std::string_view interfaceName, InterfaceType type,
                    std::string_view topic, uint64_t bytes) override
   {
      messagesSentCounter().Add(1, attributes(interfaceName, type, topic));
      bytesSentCounter().Add(bytes, attributes(interfaceName, type, topic));
   }

   void recordReceived(std::string_view interfaceName, InterfaceType type,
                        std::string_view topic, uint64_t bytes) override
   {
      messagesReceivedCounter().Add(1, attributes(interfaceName, type, topic));
      bytesReceivedCounter().Add(bytes, attributes(interfaceName, type, topic));
   }

private:
   static std::map<std::string, std::string> attributes(std::string_view interfaceName,
                                                          InterfaceType type,
                                                          std::string_view topic)
   {
      return {
         {"interface.name", std::string(interfaceName)},
         {"interface.type", std::string(toString(type))},
         {"topic", std::string(topic)},
      };
   }

   static opentelemetry::metrics::Counter<uint64_t> &messagesSentCounter()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> counter =
         opentelemetry::metrics::Provider::GetMeterProvider()
            ->GetMeter("Observability")
            ->CreateUInt64Counter("interface.messages.sent", "Number of messages sent", "{message}");
      return *counter;
   }

   static opentelemetry::metrics::Counter<uint64_t> &messagesReceivedCounter()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> counter =
         opentelemetry::metrics::Provider::GetMeterProvider()
            ->GetMeter("Observability")
            ->CreateUInt64Counter("interface.messages.received", "Number of messages received", "{message}");
      return *counter;
   }

   static opentelemetry::metrics::Counter<uint64_t> &bytesSentCounter()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> counter =
         opentelemetry::metrics::Provider::GetMeterProvider()
            ->GetMeter("Observability")
            ->CreateUInt64Counter("interface.bytes.sent", "Number of bytes sent", "By");
      return *counter;
   }

   static opentelemetry::metrics::Counter<uint64_t> &bytesReceivedCounter()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> counter =
         opentelemetry::metrics::Provider::GetMeterProvider()
            ->GetMeter("Observability")
            ->CreateUInt64Counter("interface.bytes.received", "Number of bytes received", "By");
      return *counter;
   }
};

} // namespace

std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> init(const MetricsOptions &options)
{
   namespace otlp = opentelemetry::exporter::otlp;
   namespace metrics_sdk = opentelemetry::sdk::metrics;
   namespace metrics_api = opentelemetry::metrics;
   namespace resource = opentelemetry::sdk::resource;

   otlp::OtlpGrpcMetricExporterOptions exporterOptions;
   auto exporter = otlp::OtlpGrpcMetricExporterFactory::Create(exporterOptions);

   metrics_sdk::PeriodicExportingMetricReaderOptions readerOptions;
   readerOptions.export_interval_millis = options.aggregationPeriod;
   readerOptions.export_timeout_millis = options.aggregationPeriod;
   auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), readerOptions);

   auto context = metrics_sdk::MeterContextFactory::Create(
      metrics_sdk::ViewRegistryFactory::Create(),
      resource::Resource::Create({{"service.name", options.serviceName}}));
   context->AddMetricReader(std::move(reader));

   // MeterProviderFactory::Create() returns the API base type by design, so
   // that library code stays decoupled from the concrete SDK. We keep an SDK-
   // typed alias (sharing the same control block) so main() can still call
   // the SDK-only ForceFlush()/Shutdown() on the same object before exit.
   std::shared_ptr<metrics_api::MeterProvider> apiProvider(metrics_sdk::MeterProviderFactory::Create(std::move(context)));
   auto sdkProvider = std::static_pointer_cast<metrics_sdk::MeterProvider>(apiProvider);

   metrics_api::Provider::SetMeterProvider(apiProvider);

   return sdkProvider;
}

IInterfaceMetrics &metrics()
{
   static OtelInterfaceMetrics instance;
   return instance;
}

} // namespace Observability
