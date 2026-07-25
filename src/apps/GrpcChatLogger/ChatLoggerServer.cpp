/**
 * @file ChatLoggerServer.cpp
 * @brief GrpcChatLogger server — stores the latest 100 reported chat
 * messages and serves the most recent one back to readers.
 */

#include "ChatMessageStore.h"

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
#include <memory>
#include <string>
#include <thread>

static std::atomic<bool> isRunning{true};
void signalHandler(int)
{
   isRunning.store(false);
}

namespace
{

class ChatLoggerServiceImpl final : public chatlogger::ChatLogger::Service
{
public:
   grpc::Status ReportMessage(grpc::ServerContext *, const chatlogger::ReportRequest *request,
                               chatlogger::ReportResponse *response) override
   {
      Observability::metrics().recordReceived(
         "GrpcChatLoggerServer", Observability::InterfaceType::GRPC, "chat.report", request->ByteSizeLong());

      auto stored = _store.push(request->text());
      response->set_id(stored.id);

      GPINFO("[ReportMessage] id={} text=\"{}\"", stored.id, stored.text);

      Observability::metrics().recordSent(
         "GrpcChatLoggerServer", Observability::InterfaceType::GRPC, "chat.report", response->ByteSizeLong());
      return grpc::Status::OK;
   }

   grpc::Status GetLatestMessage(grpc::ServerContext *, const chatlogger::GetLatestRequest *request,
                                  chatlogger::GetLatestResponse *response) override
   {
      Observability::metrics().recordReceived(
         "GrpcChatLoggerServer", Observability::InterfaceType::GRPC, "chat.latest", request->ByteSizeLong());

      auto latest = _store.latest();
      if (latest)
      {
         auto *message = response->mutable_message();
         message->set_id(latest->id);
         message->set_text(latest->text);
         message->set_timestamp_ms(latest->timestampMs);
      }

      Observability::metrics().recordSent(
         "GrpcChatLoggerServer", Observability::InterfaceType::GRPC, "chat.latest", response->ByteSizeLong());
      return grpc::Status::OK;
   }

private:
   GrpcChatLogger::ChatMessageStore _store;
};

} // namespace

// NOLINTNEXTLINE
int main(int argc, char *argv[])
{
   (void)std::signal(SIGINT, signalHandler);
   (void)std::signal(SIGTERM, signalHandler);

   CommonUtils::GeneralLogger logger;
   logger.init("GrpcChatLoggerServer");

   auto otelMeterProvider = Observability::init({.serviceName = "GrpcChatLoggerServer"});
   auto otelLoggerProvider = Observability::initLogging({.serviceName = "GrpcChatLoggerServer"});
   CommonUtils::GeneralLogger::addSink(Observability::createOtelLogSink());

   // Reuses the MeterProvider (and therefore the OTLP exporter/collector
   // pipeline) Observability::init() just registered globally -- gRPC's
   // own RPC-level metrics (call duration, message sizes, status codes)
   // ride the same pipeline as the interface.messages.* counters above,
   // with no separate exporter setup. Must run before any Server/Channel
   // is created.
   auto otelPluginStatus = grpc::OpenTelemetryPluginBuilder()
                               .SetMeterProvider(opentelemetry::metrics::Provider::GetMeterProvider())
                               .BuildAndRegisterGlobal();
   if (!otelPluginStatus.ok())
   {
      GPWARN("Failed to register gRPC OpenTelemetry plugin: {}", otelPluginStatus.ToString());
   }

   std::string listenAddress = "0.0.0.0:50051";
   if (argc > 1)
   {
      listenAddress = argv[1];
   }
   GPINFO("GrpcChatLoggerServer listening on {}", listenAddress);

   ChatLoggerServiceImpl service;

   grpc::ServerBuilder builder;
   builder.AddListeningPort(listenAddress, grpc::InsecureServerCredentials());
   builder.RegisterService(&service);

   std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
   if (!server)
   {
      GPWARN("GrpcChatLoggerServer failed to start on {}", listenAddress);
      return 1;
   }

   std::thread waitThread([&server]() { server->Wait(); });

   GPINFO("GrpcChatLoggerServer running. Press Ctrl+C to stop.");
   while (isRunning.load())
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }

   GPINFO("GrpcChatLoggerServer shutting down");
   server->Shutdown();
   waitThread.join();

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
