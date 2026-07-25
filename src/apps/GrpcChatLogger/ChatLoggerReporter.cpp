/**
 * @file ChatLoggerReporter.cpp
 * @brief GrpcChatLogger client that periodically reports a new chat message
 * to the server via RPC.
 */

#include "CommonUtils/GeneralLogger.h"
#include "Observability/InterfaceMetrics.h"
#include "Observability/OtelLogSink.h"

#include "chat_logger.grpc.pb.h"

#include <grpcpp/ext/otel_plugin.h>
#include <grpcpp/grpcpp.h>
#include <opentelemetry/metrics/provider.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

static std::atomic<bool> isRunning{true};
void signalHandler(int)
{
   isRunning.store(false);
}

static void sleepResponsive(std::chrono::milliseconds total)
{
   auto deadline = std::chrono::steady_clock::now() + total;
   while (isRunning.load() && std::chrono::steady_clock::now() < deadline)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }
}

// NOLINTNEXTLINE
int main(int argc, char *argv[])
{
   (void)std::signal(SIGINT, signalHandler);
   (void)std::signal(SIGTERM, signalHandler);

   CommonUtils::GeneralLogger logger;
   logger.init("GrpcChatLoggerReporter");

   auto otelMeterProvider = Observability::init({.serviceName = "GrpcChatLoggerReporter"});
   auto otelLoggerProvider = Observability::initLogging({.serviceName = "GrpcChatLoggerReporter"});
   CommonUtils::GeneralLogger::addSink(Observability::createOtelLogSink());

   // Reuses the MeterProvider Observability::init() just registered
   // globally. Must run before any Channel is created.
   auto otelPluginStatus = grpc::OpenTelemetryPluginBuilder()
                               .SetMeterProvider(opentelemetry::metrics::Provider::GetMeterProvider())
                               .BuildAndRegisterGlobal();
   if (!otelPluginStatus.ok())
   {
      GPWARN("Failed to register gRPC OpenTelemetry plugin: {}", otelPluginStatus.ToString());
   }

   std::string serverAddress = "localhost:50051";
   if (argc > 1)
   {
      serverAddress = argv[1];
   }
   GPINFO("GrpcChatLoggerReporter connecting to {}", serverAddress);

   auto channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());
   auto stub = chatlogger::ChatLogger::NewStub(channel);

   int64_t messageCount = 0;
   GPINFO("GrpcChatLoggerReporter running. Press Ctrl+C to stop.");
   while (isRunning.load())
   {
      chatlogger::ReportRequest request;
      request.set_text("Reporter message #" + std::to_string(messageCount++));

      chatlogger::ReportResponse response;
      grpc::ClientContext context;

      Observability::metrics().recordSent(
         "GrpcChatLoggerReporter", Observability::InterfaceType::GRPC, "chat.report", request.ByteSizeLong());

      grpc::Status status = stub->ReportMessage(&context, request, &response);
      if (status.ok())
      {
         Observability::metrics().recordReceived(
            "GrpcChatLoggerReporter", Observability::InterfaceType::GRPC, "chat.report", response.ByteSizeLong());
         GPINFO("[ReportMessage] sent text=\"{}\" assigned id={}", request.text(), response.id());
      }
      else
      {
         GPWARN("[ReportMessage] RPC failed: {}", status.error_message());
      }

      sleepResponsive(std::chrono::seconds(2));
   }

   GPINFO("GrpcChatLoggerReporter shutting down after {} messages", messageCount);

   // Flush any pending export synchronously. Shutdown() is intentionally not
   // called here: the SDK's MeterProvider/LoggerProvider destructors already
   // shut themselves down exactly once when the last reference (held by the
   // global opentelemetry::metrics::Provider/opentelemetry::logs::Provider)
   // is released at process exit, and calling Shutdown() a second time there
   // is undefined behavior.
   otelMeterProvider->ForceFlush();
   otelLoggerProvider->ForceFlush();

   return 0;
}
