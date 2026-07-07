/**
 * @file GrpcClientTest.cpp
 * @brief Test application for a gRPC client.
 *
 * Continuously exercises all three services: SensorService, CommandService,
 * and DiagnosticsService until Ctrl-C.  Handles server disconnections and
 * automatically reconnects when the server comes back.
 */

#include "GeneralLogger.h"

#include <sensor_service.grpc.pb.h>
#include <sensor_service.pb.h>

#include <google/protobuf/empty.pb.h>
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <csignal>
#include <random>
#include <string>
#include <thread>

static std::atomic<bool> sRunning{true};

void signalHandler(int)
{
   sRunning.store(false);
}

// ============================================================================
//  Helper: build a SensorReading
// ============================================================================

grpc_sensor::SensorReading makeReading(const std::string &sensorId,
                                       const std::string &name,
                                       double value,
                                       const std::string &unit)
{
   grpc_sensor::SensorReading r;
   r.set_sensor_id(sensorId);
   r.set_sensor_name(name);
   r.set_value(value);
   r.set_unit(unit);
   auto now = std::chrono::system_clock::now();
   r.set_timestamp_ns(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
   return r;
}

// ============================================================================
//  SensorService exercises
// ============================================================================

void testSensorService(const std::shared_ptr<grpc::Channel> &channel)
{
   auto stub = grpc_sensor::SensorService::NewStub(channel);

   GPINFO("=== SensorService ===");

   // --- Unary: submit several readings --------------------------------------

   std::random_device rd;
   std::mt19937 gen(rd());
   std::uniform_real_distribution<> tempDist(18.0, 32.0);
   std::uniform_real_distribution<> pressureDist(980.0, 1040.0);

   constexpr int NUM_READINGS = 5;

   for (int i = 0; i < NUM_READINGS && sRunning.load(); ++i)
   {
      grpc_sensor::SubmitReadingRequest request;

      if (i % 2 == 0)
      {
         *request.mutable_reading() = makeReading(
            "temp-001", "Temperature Sensor", tempDist(gen), "celsius");
      }
      else
      {
         *request.mutable_reading() = makeReading(
            "press-002", "Pressure Sensor", pressureDist(gen), "hPa");
      }

      grpc_sensor::SubmitReadingResponse response;
      grpc::ClientContext ctx;

      auto status = stub->SubmitReading(&ctx, request, &response);
      if (status.ok())
      {
         GPINFO("SubmitReading[{}]: accepted={} detail='{}'",
                i, response.accepted(), response.detail());
      }
      else
      {
         GPERROR("SubmitReading[{}] failed: {} — {}",
                 i, static_cast<int>(status.error_code()), status.error_message());
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(200));
   }

   // --- Server-streaming: request all stored readings -----------------------

   {
      GPINFO("Requesting stream of stored readings...");

      grpc_sensor::StreamReadingsRequest streamReq;
      streamReq.set_max_count(10);

      grpc::ClientContext streamCtx;
      auto reader = stub->StreamReadings(&streamCtx, streamReq);

      grpc_sensor::SensorReading reading;
      int count = 0;
      while (reader->Read(&reading))
      {
         GPINFO("  Streamed: sensor_id={} value={:.2f} {}",
                reading.sensor_id(), reading.value(), reading.unit());
         ++count;
      }

      auto streamStatus = reader->Finish();
      if (streamStatus.ok())
      {
         GPINFO("StreamReadings finished — received {} readings", count);
      }
      else
      {
         GPERROR("StreamReadings failed: {} — {}",
                 static_cast<int>(streamStatus.error_code()),
                 streamStatus.error_message());
      }
   }

   // --- Fire-and-forget: ClearReadings (Empty → Empty) ----------------------

   {
      GPINFO("Calling ClearReadings (fire-and-forget)...");
      grpc::ClientContext ctx;
      const google::protobuf::Empty req;
      google::protobuf::Empty resp;
      auto status = stub->ClearReadings(&ctx, req, &resp);
      if (status.ok())
      {
         GPINFO("ClearReadings succeeded");
      }
      else
      {
         GPERROR("ClearReadings failed: {}", status.error_message());
      }
   }
}

// ============================================================================
//  CommandService exercises
// ============================================================================

void testCommandService(const std::shared_ptr<grpc::Channel> &channel)
{
   auto stub = grpc_sensor::CommandService::NewStub(channel);

   GPINFO("=== CommandService ===");

   // --- Unary: ExecuteCommand -----------------------------------------------

   {
      grpc_sensor::CommandRequest request;
      request.set_command_id("cmd-1001");
      request.set_type(grpc_sensor::COMMAND_START);
      request.set_target_id("sensor-cluster-A");
      request.set_payload("initial-config");

      grpc_sensor::CommandResponse response;
      grpc::ClientContext ctx;

      auto status = stub->ExecuteCommand(&ctx, request, &response);
      if (status.ok())
      {
         GPINFO("ExecuteCommand: id={} success={} message='{}'",
                response.command_id(), response.success(), response.message());
      }
      else
      {
         GPERROR("ExecuteCommand failed: {}", status.error_message());
      }
   }

   // --- Fire-and-forget: FireCommand (returns Empty) ------------------------

   {
      grpc_sensor::CommandRequest request;
      request.set_command_id("cmd-1002");
      request.set_type(grpc_sensor::COMMAND_RESET);
      request.set_target_id("sensor-cluster-B");

      grpc::ClientContext ctx;
      google::protobuf::Empty resp;

      auto status = stub->FireCommand(&ctx, request, &resp);
      if (status.ok())
      {
         GPINFO("FireCommand: sent (fire-and-forget, no response payload)");
      }
      else
      {
         GPERROR("FireCommand failed: {}", status.error_message());
      }
   }
}

// ============================================================================
//  DiagnosticsService exercises
// ============================================================================

void testDiagnosticsService(const std::shared_ptr<grpc::Channel> &channel)
{
   auto stub = grpc_sensor::DiagnosticsService::NewStub(channel);

   GPINFO("=== DiagnosticsService ===");

   // --- Unary: CheckHealth --------------------------------------------------

   {
      grpc_sensor::HealthCheckRequest request;
      request.set_service_name("SensorService");

      grpc_sensor::HealthCheckResponse response;
      grpc::ClientContext ctx;

      auto status = stub->CheckHealth(&ctx, request, &response);
      if (status.ok())
      {
         GPINFO("CheckHealth: status={} details='{}' uptime_ms={}",
                grpc_sensor::HealthCheckResponse::Status_Name(response.status()),
                response.details(),
                response.uptime_ms());
      }
      else
      {
         GPERROR("CheckHealth failed: {}", status.error_message());
      }
   }

   // --- Client-streaming: IngestLogs ----------------------------------------

   {
      GPINFO("IngestLogs: sending log entries (client-streaming)...");

      grpc::ClientContext ctx;
      grpc_sensor::LogSummary summary;
      auto writer = stub->IngestLogs(&ctx, &summary);

      struct LogSample
      {
         grpc_sensor::Severity severity;
         std::string source;
         std::string message;
      };

      const std::vector<LogSample> logs = {
         {grpc_sensor::SEVERITY_INFO,    "sensor-001", "Sensor started"},
         {grpc_sensor::SEVERITY_WARNING, "sensor-002", "Battery low"},
         {grpc_sensor::SEVERITY_ERROR,   "sensor-003", "Connection lost"},
         {grpc_sensor::SEVERITY_INFO,    "sensor-001", "Reading captured"},
         {grpc_sensor::SEVERITY_ERROR,   "sensor-004", "Checksum mismatch"},
      };

      for (const auto &log : logs)
      {
         grpc_sensor::LogEntry entry;
         entry.set_severity(log.severity);
         entry.set_source(log.source);
         entry.set_message(log.message);
         auto now = std::chrono::system_clock::now();
         entry.set_timestamp_ns(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
               now.time_since_epoch()).count());

         if (!writer->Write(entry))
         {
            GPERROR("IngestLogs: write failed (broken stream)");
            break;
         }
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      writer->WritesDone();
      auto status = writer->Finish();
      if (status.ok())
      {
         GPINFO("IngestLogs summary: received={} errors={} warnings={}",
                summary.entries_received(), summary.errors(), summary.warnings());
      }
      else
      {
         GPERROR("IngestLogs failed: {}", status.error_message());
      }
   }

   // --- Fire-and-forget: ResetCounters (Empty → Empty) ----------------------

   {
      GPINFO("Calling ResetCounters (fire-and-forget)...");
      grpc::ClientContext ctx;
      const google::protobuf::Empty req;
      google::protobuf::Empty resp;
      auto status = stub->ResetCounters(&ctx, req, &resp);
      if (status.ok())
      {
         GPINFO("ResetCounters succeeded");
      }
      else
      {
         GPERROR("ResetCounters failed: {}", status.error_message());
      }
   }
}

// ============================================================================
//  Connection helper
// ============================================================================

bool waitForReady(const std::shared_ptr<grpc::Channel> &channel,
                  const std::string &serverAddr)
{
   while (sRunning.load())
   {
      auto state = channel->GetState(true);
      if (state == GRPC_CHANNEL_READY)
      {
         return true;
      }
      if (state == GRPC_CHANNEL_SHUTDOWN)
      {
         GPERROR("Channel entered SHUTDOWN state");
         return false;
      }
      GPINFO("Channel state: {} — waiting for server at {} ...",
             static_cast<int>(state), serverAddr);
      channel->WaitForStateChange(
         state, std::chrono::system_clock::now() + std::chrono::seconds(1));
   }
   return false;
}

// ============================================================================
//  main
// ============================================================================

// NOLINTNEXTLINE
int main(int argc, char *argv[])
{
   (void)std::signal(SIGINT, signalHandler);
   (void)std::signal(SIGTERM, signalHandler);

   CommonUtils::GeneralLogger logger;
   logger.init("GrpcClientTest");

   std::string serverAddr = "localhost:50051";
   if (argc > 1)
   {
      serverAddr = argv[1];
   }

   GPINFO("Connecting to gRPC server at {}", serverAddr);

   auto channel = grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials());

   int round = 0;
   while (sRunning.load())
   {
      if (!waitForReady(channel, serverAddr))
      {
         break;
      }

      ++round;
      GPINFO("===== Round {} =====", round);

      testSensorService(channel);
      testCommandService(channel);
      testDiagnosticsService(channel);

      GPINFO("Round {} complete — pausing before next round", round);

      // Pace the loop; break early on shutdown
      for (int i = 0; i < 10 && sRunning.load(); ++i)
      {
         std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
   }

   GPINFO("Client shut down after {} rounds", round);
   return 0;
}
