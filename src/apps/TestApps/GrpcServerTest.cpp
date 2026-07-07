/**
 * @file GrpcServerTest.cpp
 * @brief Test application for a gRPC server.
 *
 * Implements SensorService, CommandService, and DiagnosticsService
 * from sensor_service.proto.
 */

#include "GeneralLogger.h"

#include <sensor_service.grpc.pb.h>
#include <sensor_service.pb.h>

#include <google/protobuf/empty.pb.h>
#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> sRunning{true};
static grpc::Server *sServer{nullptr};

void signalHandler(int)
{
   sRunning.store(false);
   if (sServer != nullptr)
   {
      sServer->Shutdown();
   }
}

// ============================================================================
//  SensorService implementation
// ============================================================================

class SensorServiceImpl final : public grpc_sensor::SensorService::Service
{
public:
   grpc::Status SubmitReading(
      grpc::ServerContext * /*context*/,
      const grpc_sensor::SubmitReadingRequest *request,
      grpc_sensor::SubmitReadingResponse *response) override
   {
      const auto &r = request->reading();
      GPINFO("SubmitReading: sensor_id={} value={:.2f} {}", r.sensor_id(), r.value(), r.unit());

      {
         const std::lock_guard lock(_mutex);
         _readings.push_back(r);
      }

      response->set_accepted(true);
      response->set_detail("Stored reading for " + r.sensor_id());
      return grpc::Status::OK;
   }

   grpc::Status StreamReadings(
      grpc::ServerContext *context,
      const grpc_sensor::StreamReadingsRequest *request,
      grpc::ServerWriter<grpc_sensor::SensorReading> *writer) override
   {
      const auto &filter = request->sensor_id_filter();
      int32_t maxCount = request->max_count();
      if (maxCount <= 0)
      {
         maxCount = 10;
      }

      GPINFO("StreamReadings: filter='{}' max_count={}", filter, maxCount);

      std::vector<grpc_sensor::SensorReading> snapshot;
      {
         const std::lock_guard lock(_mutex);
         snapshot = _readings;
      }

      int32_t sent = 0;
      for (const auto &r : snapshot)
      {
         if (context->IsCancelled())
         {
            //NOLINTNEXTLINE
            return grpc::Status(grpc::CANCELLED, "Client cancelled");
         }
         if (!filter.empty() && r.sensor_id() != filter)
         {
            continue;
         }
         writer->Write(r);
         ++sent;
         if (sent >= maxCount)
         {
            break;
         }
      }

      GPINFO("StreamReadings: sent {} readings", sent);
      return grpc::Status::OK;
   }

   grpc::Status ClearReadings(
      grpc::ServerContext * /*context*/,
      const google::protobuf::Empty * /*request*/,
      google::protobuf::Empty * /*response*/) override
   {
      const std::lock_guard lock(_mutex);
      auto count = _readings.size();
      _readings.clear();
      GPINFO("ClearReadings: purged {} stored readings", count);
      return grpc::Status::OK;
   }

private:
   std::mutex _mutex;
   std::vector<grpc_sensor::SensorReading> _readings;
};

// ============================================================================
//  CommandService implementation
// ============================================================================

class CommandServiceImpl final : public grpc_sensor::CommandService::Service
{
public:
   grpc::Status ExecuteCommand(
      grpc::ServerContext * /*context*/,
      const grpc_sensor::CommandRequest *request,
      grpc_sensor::CommandResponse *response) override
   {
      GPINFO("ExecuteCommand: id='{}' type={} target='{}'",
             request->command_id(),
             static_cast<int>(request->type()),
             request->target_id());

      response->set_command_id(request->command_id());
      response->set_success(true);
      response->set_message("Executed " + grpc_sensor::CommandType_Name(request->type())
                            + " on " + request->target_id());
      return grpc::Status::OK;
   }

   grpc::Status FireCommand(
      grpc::ServerContext * /*context*/,
      const grpc_sensor::CommandRequest *request,
      google::protobuf::Empty * /*response*/) override
   {
      GPINFO("FireCommand (fire-and-forget): id='{}' type={} target='{}'",
             request->command_id(),
             static_cast<int>(request->type()),
             request->target_id());
      // No response payload — fire-and-forget
      return grpc::Status::OK;
   }
};

// ============================================================================
//  DiagnosticsService implementation
// ============================================================================

class DiagnosticsServiceImpl final : public grpc_sensor::DiagnosticsService::Service
{
public:
   DiagnosticsServiceImpl()
      : _startTime(std::chrono::steady_clock::now())
   {
   }

   grpc::Status CheckHealth(
      grpc::ServerContext * /*context*/,
      const grpc_sensor::HealthCheckRequest *request,
      grpc_sensor::HealthCheckResponse *response) override
   {
      GPINFO("CheckHealth: service='{}'", request->service_name());

      auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
         std::chrono::steady_clock::now() - _startTime);

      response->set_status(grpc_sensor::HealthCheckResponse::SERVING);
      response->set_details("All systems nominal");
      response->set_uptime_ms(uptime.count());
      return grpc::Status::OK;
   }

   grpc::Status IngestLogs(
      grpc::ServerContext * /*context*/,
      grpc::ServerReader<grpc_sensor::LogEntry> *reader,
      grpc_sensor::LogSummary *response) override
   {
      GPINFO("IngestLogs: client-streaming started");

      grpc_sensor::LogEntry entry;
      int32_t total = 0;
      int32_t errors = 0;
      int32_t warnings = 0;

      while (reader->Read(&entry))
      {
         ++total;
         if (entry.severity() == grpc_sensor::SEVERITY_ERROR)
         {
            ++errors;
         }
         else if (entry.severity() == grpc_sensor::SEVERITY_WARNING)
         {
            ++warnings;
         }

         GPINFO("  Log[{}]: [{}] {} — {}",
                total,
                grpc_sensor::Severity_Name(entry.severity()),
                entry.source(),
                entry.message());
      }

      response->set_entries_received(total);
      response->set_errors(errors);
      response->set_warnings(warnings);

      GPINFO("IngestLogs: received {} entries ({} errors, {} warnings)",
             total, errors, warnings);
      return grpc::Status::OK;
   }

   grpc::Status ResetCounters(
      grpc::ServerContext * /*context*/,
      const google::protobuf::Empty * /*request*/,
      google::protobuf::Empty * /*response*/) override
   {
      GPINFO("ResetCounters: diagnostics counters cleared (fire-and-forget)");
      _startTime = std::chrono::steady_clock::now();
      return grpc::Status::OK;
   }

private:
   std::chrono::steady_clock::time_point _startTime;
};

// ============================================================================
//  main
// ============================================================================

// NOLINTNEXTLINE
int main(int argc, char *argv[])
{
   (void)std::signal(SIGINT, signalHandler);
   (void)std::signal(SIGTERM, signalHandler);

   CommonUtils::GeneralLogger logger;
   logger.init("GrpcServerTest");

   std::string listenAddr = "0.0.0.0:50051";
   if (argc > 1)
   {
      listenAddr = argv[1];
   }

   GPINFO("Starting gRPC server on {}", listenAddr);

   SensorServiceImpl sensorService;
   CommandServiceImpl commandService;
   DiagnosticsServiceImpl diagnosticsService;

   grpc::ServerBuilder builder;
   builder.AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
   builder.RegisterService(&sensorService);
   builder.RegisterService(&commandService);
   builder.RegisterService(&diagnosticsService);

   auto server = builder.BuildAndStart();
   if (!server)
   {
      GPERROR("Failed to start gRPC server on {}", listenAddr);
      return 1;
   }

   sServer = server.get();
   GPINFO("gRPC server listening on {}", listenAddr);

   server->Wait();

   GPINFO("gRPC server shut down");
   return 0;
}
