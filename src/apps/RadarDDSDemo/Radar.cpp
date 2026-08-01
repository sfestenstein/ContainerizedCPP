/**
 * @file Radar.cpp
 * @brief RadarDDSDemo radar app.
 *
 * Receives Command and replies with CommandStatus; publishes RadarTrack
 * (Best Effort), ComponentStatus (TransientLocal, periodic), and RadarAlert
 * (TransientLocal, rare).
 *
 * Pass --stress to fire an unthrottled burst of RadarTrack samples at
 * startup. At light publish rates on loopback, Best Effort delivery may
 * never actually lose anything; the burst forces the reader's KeepLast(1)
 * history to overwrite samples faster than the Workstation's listener
 * thread can drain them, producing observable sequence-number gaps.
 */

#include "CommonUtils/GeneralLogger.h"
#include "CycloneDDS/DDSPublisher.h"
#include "CycloneDDS/DDSSubscriber.h"
#include "RadarTopics.h"

#include "Command.hpp"
#include "CommandStatus.hpp"
#include "ComponentStatus.hpp"
#include "RadarAlert.hpp"
#include "RadarTrack.hpp"

#include "Observability/MetricsPipeline.h"
#include "Observability/OtelLogSink.h"
#include "Observability/ProcessMetrics.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

using radar_demo::Command;
using radar_demo::CommandStatus;
using radar_demo::ComponentStatus;
using radar_demo::RadarAlert;
using radar_demo::RadarTrack;

static std::atomic<bool> isRunning{true};
void signalHandler(int)
{
   isRunning.store(false);
}

static int64_t nowNs()
{
   return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

static void printBanner()
{
   GPINFO("RadarDDSDemo Radar — topic QoS profiles:");
   GPINFO("  RadarCommand         : Reliable,    Volatile,       KeepLast(10)");
   GPINFO("  RadarCommandStatus   : Reliable,    Volatile,       KeepLast(10)");
   GPINFO("  RadarTrack           : Best Effort, Volatile,       KeepLast(1)");
   GPINFO("  RadarComponentStatus : Reliable,    TransientLocal, KeepLast(1)");
   GPINFO("  RadarAlert           : Reliable,    TransientLocal, KeepAll(<=50/instance)");
}

namespace
{

struct SimulatedTrack
{
   int32_t trackId;
   double range_m;
   double azimuth_deg;
   double elevation_deg;
   int32_t sequenceNumber = 0;
};

} // namespace

// NOLINTNEXTLINE
int main(int argc, char *argv[])
{
   (void)std::signal(SIGINT, signalHandler);
   (void)std::signal(SIGTERM, signalHandler);

   CommonUtils::GeneralLogger logger;
   logger.init("RadarDDSRadar");

   auto otelMeterProvider = Observability::initMetrics({.serviceName = "RadarDDSRadar"});
   auto otelLoggerProvider = Observability::initLogging({.serviceName = "RadarDDSRadar"});
   CommonUtils::GeneralLogger::addSink(Observability::createOtelLogSink());
   Observability::ProcessMetrics processMetrics(otelMeterProvider->GetMeter("Observability"));

   uint32_t domainId = 0;
   bool stressMode = false;
   for (int i = 1; i < argc; ++i)
   {
      std::string arg = argv[i];
      if (arg == "--stress")
      {
         stressMode = true;
      }
      else
      {
         domainId = static_cast<uint32_t>(std::stoul(arg));
      }
   }
   GPINFO("Using DDS domain ID: {}, stress mode: {}", domainId, stressMode);
   printBanner();

   RadarDemo::RadarTopics topics;
   const auto &config = topics.config();

   // Constructed and warmed up before subscribing to Command, so the real
   // ack path in the Command listener callback never triggers lazy
   // Topic/DataWriter creation from within that callback.
   CycloneDDS::DDSPublisher<CommandStatus> commandStatusPub(
      domainId, config.getEntry(std::string(RadarDemo::COMMAND_STATUS_TOPIC)), "RadarCommandStatusPub");
   {
      CommandStatus startup;
      startup.header().sender_id("Radar");
      startup.header().timestamp_ns(nowNs());
      startup.command_id("STARTUP");
      startup.result(radar_demo::CommandResult::CMDSTATUS_ACCEPTED);
      startup.detail("Radar online");
      commandStatusPub.publish(startup);
      GPINFO("[CommandStatus] writer warmed up (Radar online)");
   }

   CycloneDDS::DDSPublisher<RadarTrack> trackPub(
      domainId, config.getEntry(std::string(RadarDemo::RADAR_TRACK_TOPIC)), "RadarTrackPub");
   CycloneDDS::DDSPublisher<ComponentStatus> componentPub(
      domainId, config.getEntry(std::string(RadarDemo::COMPONENT_STATUS_TOPIC)), "RadarComponentStatusPub");
   CycloneDDS::DDSPublisher<RadarAlert> alertPub(
      domainId, config.getEntry(std::string(RadarDemo::RADAR_ALERT_TOPIC)), "RadarAlertPub");

   CycloneDDS::DDSSubscriber<Command> commandSub(
      domainId, config.getEntry(std::string(RadarDemo::COMMAND_TOPIC)), "RadarCommandSub");
   commandSub.subscribe(
      [&commandStatusPub](const Command &cmd)
      {
         GPINFO("[Command] received command_id={} type={} target={}",
                cmd.command_id(), static_cast<int>(cmd.type()), cmd.target_id());

         CommandStatus status;
         status.header().sender_id("Radar");
         status.header().timestamp_ns(nowNs());
         status.command_id(cmd.command_id());
         status.result(radar_demo::CommandResult::CMDSTATUS_ACCEPTED);
         status.detail("Command executed");
         commandStatusPub.publish(status);
      });
   commandSub.start();

   std::random_device rd;
   std::mt19937 gen(rd());
   std::uniform_real_distribution<> jitter(-0.5, 0.5);

   std::vector<SimulatedTrack> tracks = {
      {1001, 12000.0, 45.0, 5.0, 0},
      {1002, 8500.0, 120.0, 12.0, 0},
      {1003, 20000.0, 270.0, 2.0, 0},
   };

   auto publishTrackBatch = [&]()
   {
      for (auto &t : tracks)
      {
         t.range_m += jitter(gen) * 20.0;
         t.azimuth_deg += jitter(gen);
         t.elevation_deg += jitter(gen) * 0.1;

         RadarTrack msg;
         msg.header().sender_id("Radar");
         msg.header().timestamp_ns(nowNs());
         msg.track_id(t.trackId);
         msg.range_m(t.range_m);
         msg.azimuth_deg(t.azimuth_deg);
         msg.elevation_deg(t.elevation_deg);
         msg.radial_velocity_mps(jitter(gen) * 50.0);
         msg.confidence(0.9);
         msg.status(radar_demo::TrackState::TRACK_UPDATED);
         msg.sequence_number(t.sequenceNumber++);

         trackPub.publish(msg);
      }
   };

   if (stressMode)
   {
      GPINFO("[stress] firing unthrottled RadarTrack burst to force best-effort drops");
      for (int i = 0; i < 500; ++i)
      {
         publishTrackBatch();
      }
      GPINFO("[stress] burst complete ({} samples per track), entering steady state", 500);
   }

   const std::vector<std::string> components = {"transmitter", "receiver", "antenna_servo", "cooling"};
   int32_t alertSequence = 0;

   auto lastTrackPublish = std::chrono::steady_clock::now();
   auto lastComponentPublish = std::chrono::steady_clock::now();
   auto lastAlertPublish = std::chrono::steady_clock::now();

   GPINFO("RadarDDSRadar running. Press Ctrl+C to stop.");

   while (isRunning.load())
   {
      auto now = std::chrono::steady_clock::now();

      if (now - lastTrackPublish >= std::chrono::milliseconds(200))
      {
         publishTrackBatch();
         lastTrackPublish = now;
      }

      if (now - lastComponentPublish >= std::chrono::seconds(5))
      {
         for (const auto &componentId : components)
         {
            ComponentStatus cs;
            cs.header().sender_id("Radar");
            cs.header().timestamp_ns(nowNs());
            cs.component_id(componentId);
            cs.health(radar_demo::ComponentHealth::HEALTH_NOMINAL);
            cs.temperature_c(35.0 + jitter(gen) * 5.0);
            cs.voltage_v(28.0 + jitter(gen) * 0.5);
            cs.detail("nominal");
            componentPub.publish(cs);
         }
         GPINFO("[ComponentStatus] published status for {} components", components.size());
         lastComponentPublish = now;
      }

      if (now - lastAlertPublish >= std::chrono::seconds(30))
      {
         RadarAlert alert;
         alert.header().sender_id("Radar");
         alert.header().timestamp_ns(nowNs());
         alert.alert_id("alert-" + std::to_string(alertSequence++));
         alert.severity(radar_demo::AlertSeverity::ALERT_WARNING);
         alert.component_id("cooling");
         alert.message("Simulated cooling temperature spike");
         alertPub.publish(alert);
         GPINFO("[RadarAlert] published simulated alert {}", alert.alert_id());
         lastAlertPublish = now;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }

   GPINFO("RadarDDSRadar shutting down");

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
