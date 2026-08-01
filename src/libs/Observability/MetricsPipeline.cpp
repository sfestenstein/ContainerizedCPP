#include "Observability/MetricsPipeline.h"

#include "Observability/MetricsExporterFactory.h"
#include "Observability/MetricsRegistry.h"
#include "Observability/OtelInterfaceMetrics.h"

#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/aggregation/aggregation_config.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/instruments.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/meter_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
#include <opentelemetry/sdk/metrics/view/view_registry.h>
#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>
#include <opentelemetry/sdk/resource/resource.h>

#include <unistd.h>

#include <cstdlib>
#include <memory>
#include <utility>

namespace Observability
{

namespace
{

// Only one caller today (initMetrics() below); pulled out as a private
// helper rather than its own class/file for that reason -- see DESIGN.md.
opentelemetry::sdk::resource::Resource buildResource(const std::string &serviceName)
{
   char hostname[256] = {};
   // gethostname()/getpid() are POSIX/glibc, matching this container-based,
   // Linux-only project's existing conventions elsewhere.
   if (gethostname(hostname, sizeof(hostname) - 1) != 0)
   {
      hostname[0] = '\0';
   }
   std::string instanceId = std::string(hostname) + "-" + std::to_string(getpid());

   const char *environment = std::getenv("DEPLOYMENT_ENVIRONMENT");

   return opentelemetry::sdk::resource::Resource::Create({
      {"service.name", serviceName},
      {"service.version", OBSERVABILITY_SERVICE_VERSION},
      {"service.instance.id", instanceId},
      {"deployment.environment", environment != nullptr ? environment : "development"},
   });
}

// OTel's default histogram bucket boundaries are tuned for HTTP-request
// durations (seconds); loopback DDS latencies live in the
// microsecond-to-low-millisecond range, so interface.latency needs an
// explicit View with millisecond-scale boundaries to be useful for
// percentile estimation -- see DESIGN.md's "Why Views?".
std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry> buildViewRegistry()
{
   namespace metrics_sdk = opentelemetry::sdk::metrics;

   auto registry = metrics_sdk::ViewRegistryFactory::Create();

   auto boundaries = std::make_shared<metrics_sdk::HistogramAggregationConfig>();
   boundaries->boundaries_ = {0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, 25, 50, 100, 250, 500};

   auto instrumentSelector = metrics_sdk::InstrumentSelectorFactory::Create(
      metrics_sdk::InstrumentType::kHistogram, "interface.latency", "ms");
   auto meterSelector = metrics_sdk::MeterSelectorFactory::Create("Observability", "", "");
   auto view = metrics_sdk::ViewFactory::Create("interface.latency", "Send-to-receive latency", "ms",
                                                 metrics_sdk::AggregationType::kHistogram, boundaries);

   registry->AddView(std::move(instrumentSelector), std::move(meterSelector), std::move(view));
   return registry;
}

} // namespace

std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> initMetrics(const MetricsOptions &options)
{
   namespace metrics_sdk = opentelemetry::sdk::metrics;
   namespace metrics_api = opentelemetry::metrics;

   auto exporter = createMetricExporter(options.protocol, options.endpoint);

   metrics_sdk::PeriodicExportingMetricReaderOptions readerOptions;
   readerOptions.export_interval_millis = options.aggregationPeriod;
   // Must be strictly less than export_interval_millis -- the SDK silently
   // discards both and falls back to its own defaults (60s interval) if
   // they're equal or timeout is larger, which means aggregationPeriod
   // would never actually be honored (the pipeline would only ever emit
   // data via ForceFlush at shutdown, not on the configured period). The
   // config-file loader (MetricsConfig.h) validates this and fails fast;
   // this clamp is defense-in-depth for MetricsOptions built directly.
   auto exportTimeout = options.exportTimeout;
   if (exportTimeout <= std::chrono::milliseconds::zero() || exportTimeout >= options.aggregationPeriod)
   {
      exportTimeout = options.aggregationPeriod / 2;
   }
   readerOptions.export_timeout_millis = exportTimeout;
   auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), readerOptions);

   auto context =
      metrics_sdk::MeterContextFactory::Create(buildViewRegistry(), buildResource(options.serviceName));
   context->AddMetricReader(std::move(reader));

   // MeterProviderFactory::Create() returns the API base type by design, so
   // that library code stays decoupled from the concrete SDK. We keep an SDK-
   // typed alias (sharing the same control block) so main() can still call
   // the SDK-only ForceFlush()/Shutdown() on the same object before exit.
   std::shared_ptr<metrics_api::MeterProvider> apiProvider(metrics_sdk::MeterProviderFactory::Create(std::move(context)));
   auto sdkProvider = std::static_pointer_cast<metrics_sdk::MeterProvider>(apiProvider);

   metrics_api::Provider::SetMeterProvider(apiProvider);

   auto meter = apiProvider->GetMeter("Observability");
   setMetrics(std::make_shared<OtelInterfaceMetrics>(meter));

   return sdkProvider;
}

} // namespace Observability
