#include "Observability/OtelLogSink.h"

#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_factory.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_options.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/processor.h>
#include <opentelemetry/sdk/resource/resource.h>

#include <spdlog/sinks/base_sink.h>

#include <mutex>

namespace Observability
{

namespace
{

opentelemetry::logs::Severity toOtelSeverity(spdlog::level::level_enum level)
{
   switch (level)
   {
   case spdlog::level::trace:
      return opentelemetry::logs::Severity::kTrace;
   case spdlog::level::debug:
      return opentelemetry::logs::Severity::kDebug;
   case spdlog::level::info:
      return opentelemetry::logs::Severity::kInfo;
   case spdlog::level::warn:
      return opentelemetry::logs::Severity::kWarn;
   case spdlog::level::err:
      return opentelemetry::logs::Severity::kError;
   case spdlog::level::critical:
      return opentelemetry::logs::Severity::kFatal;
   case spdlog::level::off:
   case spdlog::level::n_levels:
   default:
      return opentelemetry::logs::Severity::kInfo;
   }
}

class OtelLogSink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
   void sink_it_(const spdlog::details::log_msg &msg) override
   {
      auto logger = opentelemetry::logs::Provider::GetLoggerProvider()->GetLogger("GeneralLogger");
      // Without an explicit timestamp argument, the emitted LogRecord's
      // Timestamp field defaults to the epoch (1970-01-01) -- only
      // ObservedTimestamp gets auto-populated by the SDK. spdlog::log_clock
      // is std::chrono::system_clock, so msg.time converts directly and
      // preserves the moment the log statement was actually issued (msg.time
      // is captured synchronously by spdlog; sink_it_ itself may run slightly
      // later on the async logging thread).
      logger->EmitLogRecord(toOtelSeverity(msg.level),
                             opentelemetry::nostd::string_view(msg.payload.data(), msg.payload.size()),
                             msg.time);
   }

   // The OTel batch processor flushes on its own schedule (see
   // LoggingOptions::scheduledDelay); nothing to do on spdlog's flush().
   void flush_() override
   {
   }
};

} // namespace

std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> initLogging(const LoggingOptions &options)
{
   namespace otlp = opentelemetry::exporter::otlp;
   namespace logs_sdk = opentelemetry::sdk::logs;
   namespace logs_api = opentelemetry::logs;
   namespace resource = opentelemetry::sdk::resource;

   otlp::OtlpGrpcLogRecordExporterOptions exporterOptions;
   auto exporter = otlp::OtlpGrpcLogRecordExporterFactory::Create(exporterOptions);

   logs_sdk::BatchLogRecordProcessorOptions processorOptions;
   processorOptions.schedule_delay_millis = options.scheduledDelay;
   auto processor = logs_sdk::BatchLogRecordProcessorFactory::Create(std::move(exporter), processorOptions);

   // LoggerProviderFactory::Create() returns the API base type by design, so
   // that library code stays decoupled from the concrete SDK. We keep an SDK-
   // typed alias (sharing the same control block) so main() can still call
   // the SDK-only ForceFlush() on the same object before exit.
   std::shared_ptr<logs_api::LoggerProvider> apiProvider(logs_sdk::LoggerProviderFactory::Create(
      std::move(processor), resource::Resource::Create({{"service.name", options.serviceName}})));
   auto sdkProvider = std::static_pointer_cast<logs_sdk::LoggerProvider>(apiProvider);

   logs_api::Provider::SetLoggerProvider(apiProvider);

   return sdkProvider;
}

spdlog::sink_ptr createOtelLogSink()
{
   return std::make_shared<OtelLogSink>();
}

} // namespace Observability
