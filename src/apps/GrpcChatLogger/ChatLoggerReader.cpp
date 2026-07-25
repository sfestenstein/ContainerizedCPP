/**
 * @file ChatLoggerReader.cpp
 * @brief GrpcChatLogger client that periodically polls the server for and
 * logs the latest reported chat message. Run multiple instances (with
 * distinct reader names) to demonstrate several readers observing the same
 * server.
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
#include <cstdio>
#include <csignal>
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

   if (argc < 2)
   {
      // The logger isn't initialized yet at this point, so report straight to stderr.
      std::fprintf(stderr, "Usage: %s <reader-name> [server-address]\n", argv[0]);
      return 1;
   }
   std::string readerName = argv[1];
   std::string serverAddress = argc > 2 ? argv[2] : "localhost:50051";
   std::string exeName = "GrpcChatLoggerReader_" + readerName;

   CommonUtils::GeneralLogger logger;
   logger.init(exeName);

   auto otelMeterProvider = Observability::init({.serviceName = exeName});
   auto otelLoggerProvider = Observability::initLogging({.serviceName = exeName});
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

   GPINFO("GrpcChatLoggerReader[{}] connecting to {}", readerName, serverAddress);

   auto channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());
   auto stub = chatlogger::ChatLogger::NewStub(channel);

   GPINFO("GrpcChatLoggerReader[{}] running. Press Ctrl+C to stop.", readerName);
   while (isRunning.load())
   {
      chatlogger::GetLatestRequest request;
      chatlogger::GetLatestResponse response;
      grpc::ClientContext context;

      Observability::metrics().recordSent(exeName, Observability::InterfaceType::GRPC, "chat.latest",
                                           request.ByteSizeLong());

      grpc::Status status = stub->GetLatestMessage(&context, request, &response);
      if (status.ok())
      {
         Observability::metrics().recordReceived(exeName, Observability::InterfaceType::GRPC, "chat.latest",
                                                   response.ByteSizeLong());
         if (response.has_message())
         {
            GPINFO("[{}] latest id={} text=\"{}\"", readerName, response.message().id(), response.message().text());
         }
         else
         {
            GPINFO("[{}] no message reported yet", readerName);
         }
      }
      else
      {
         GPWARN("[{}] GetLatestMessage RPC failed: {}", readerName, status.error_message());
      }

      sleepResponsive(std::chrono::seconds(1));
   }

   GPINFO("GrpcChatLoggerReader[{}] shutting down", readerName);

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
