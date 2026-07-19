#ifndef OBSERVABILITY_OTELLOGSINK_H_
#define OBSERVABILITY_OTELLOGSINK_H_

// OpenTelemetry headers
#include <opentelemetry/sdk/logs/logger_provider.h>

// spdlog headers
#include <spdlog/common.h>

// System headers
#include <chrono>
#include <memory>
#include <string>

namespace Observability
{

/**
 * @brief Options controlling the OTel logs SDK pipeline built by initLogging().
 */
struct LoggingOptions
{
   std::string serviceName;
   std::chrono::milliseconds scheduledDelay{1000};
};

/**
 * @brief Build the OTel logs SDK pipeline and register it globally.
 *
 * Builds an OTLP/gRPC log record exporter, wraps it in a batch processor
 * (flushing every options.scheduledDelay), and registers the resulting
 * LoggerProvider globally -- this is what createOtelLogSink() picks up via
 * opentelemetry::logs::Provider::GetLoggerProvider(). Call once at process
 * startup. Returns the concrete SDK provider so main() can ForceFlush() it
 * on exit.
 */
std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> initLogging(const LoggingOptions &options);

/**
 * @brief Create an spdlog sink that forwards every log record to the
 *        globally-registered OTel LoggerProvider (see initLogging()).
 *
 * Attach it to a spdlog logger via
 * CommonUtils::GeneralLogger::addSink(Observability::createOtelLogSink()).
 */
spdlog::sink_ptr createOtelLogSink();

} // namespace Observability

#endif // OBSERVABILITY_OTELLOGSINK_H_
