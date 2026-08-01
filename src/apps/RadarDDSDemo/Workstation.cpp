/**
 * @file Workstation.cpp
 * @brief RadarDDSDemo workstation app.
 *
 * Sends Command, receives CommandStatus/RadarTrack/ComponentStatus/RadarAlert.
 * Demonstrates the practical effect of each topic's QoS profile:
 *   - RadarTrack (Best Effort): tracks per-track sequence gaps to show drops.
 *   - RadarComponentStatus / RadarAlert (TransientLocal): tags the first
 *     sample received after startup as a "late-joiner snapshot" so the
 *     durability payoff is visible without comparing timestamps by hand.
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
#include <thread>

using radar_demo::Command;
using radar_demo::CommandStatus;
using radar_demo::CommandType;
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

static void sleepResponsive(std::chrono::milliseconds total)
{
   auto deadline = std::chrono::steady_clock::now() + total;
   while (isRunning.load() && std::chrono::steady_clock::now() < deadline)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }
}

static void printBanner()
{
   GPINFO("RadarDDSDemo Workstation — topic QoS profiles:");
   GPINFO("  RadarCommand         : Reliable,    Volatile,       KeepLast(10)");
   GPINFO("  RadarCommandStatus   : Reliable,    Volatile,       KeepLast(10)");
   GPINFO("  RadarTrack           : Best Effort, Volatile,       KeepLast(1)");
   GPINFO("  RadarComponentStatus : Reliable,    TransientLocal, KeepLast(1)");
   GPINFO("  RadarAlert           : Reliable,    TransientLocal, KeepAll(<=50/instance)");
}

// NOLINTNEXTLINE
int main(int argc, char *argv[])
{
   (void)std::signal(SIGINT, signalHandler);
   (void)std::signal(SIGTERM, signalHandler);

   CommonUtils::GeneralLogger logger;
   logger.init("RadarDDSWorkstation");

   auto otelMeterProvider = Observability::initMetrics(
      {.serviceName = "RadarDDSWorkstation", .protocol = Observability::ExporterProtocol::Http});
   auto otelLoggerProvider = Observability::initLogging({.serviceName = "RadarDDSWorkstation"});
   CommonUtils::GeneralLogger::addSink(Observability::createOtelLogSink());
   Observability::ProcessMetrics processMetrics(otelMeterProvider->GetMeter("Observability"));

   uint32_t domainId = 0;
   if (argc > 1)
   {
      domainId = static_cast<uint32_t>(std::stoul(argv[1]));
   }
   GPINFO("Using DDS domain ID: {}", domainId);
   printBanner();

   RadarDemo::RadarTopics topics;
   const auto &config = topics.config();

   CycloneDDS::DDSPublisher<Command> commandPub(
      domainId, config.getEntry(std::string(RadarDemo::COMMAND_TOPIC)), "WorkstationCommandPub");

   // RadarTrack drop tracking — touched only from this subscriber's own
   // polling thread, so no synchronization is needed.
   std::unordered_map<int32_t, int32_t> lastTrackSeq;
   uint64_t tracksReceived = 0;
   uint64_t trackDropsObserved = 0;

   std::atomic<bool> firstComponentStatusSeen{false};
   std::atomic<bool> firstAlertSeen{false};

   CycloneDDS::DDSSubscriber<CommandStatus> commandStatusSub(
      domainId, config.getEntry(std::string(RadarDemo::COMMAND_STATUS_TOPIC)), "WorkstationCommandStatusSub");
   commandStatusSub.subscribe(
      [](const CommandStatus &msg)
      {
         GPINFO("[CommandStatus] command_id={} result={} detail={}",
                msg.command_id(), static_cast<int>(msg.result()), msg.detail());
      });
   commandStatusSub.start();

   CycloneDDS::DDSSubscriber<RadarTrack> trackSub(
      domainId, config.getEntry(std::string(RadarDemo::RADAR_TRACK_TOPIC)), "WorkstationTrackSub");
   trackSub.subscribe(
      [&](const RadarTrack &msg)
      {
         auto it = lastTrackSeq.find(msg.track_id());
         if (it != lastTrackSeq.end() && msg.sequence_number() > it->second + 1)
         {
            trackDropsObserved += static_cast<uint64_t>(msg.sequence_number() - it->second - 1);
         }
         lastTrackSeq[msg.track_id()] = msg.sequence_number();

         if (++tracksReceived % 50 == 0)
         {
            GPINFO("[RadarTrack] received={} drops_observed={}", tracksReceived, trackDropsObserved);
         }
      });
   trackSub.start();

   CycloneDDS::DDSSubscriber<ComponentStatus> componentSub(
      domainId, config.getEntry(std::string(RadarDemo::COMPONENT_STATUS_TOPIC)), "WorkstationComponentSub");
   componentSub.subscribe(
      [&](const ComponentStatus &msg)
      {
         bool isFirst = !firstComponentStatusSeen.exchange(true);
         GPINFO("{}[ComponentStatus] component={} health={} temp={:.1f}C voltage={:.1f}V detail={}",
                isFirst ? "[late-joiner snapshot] " : "",
                msg.component_id(), static_cast<int>(msg.health()),
                msg.temperature_c(), msg.voltage_v(), msg.detail());
      });
   componentSub.start();

   CycloneDDS::DDSSubscriber<RadarAlert> alertSub(
      domainId, config.getEntry(std::string(RadarDemo::RADAR_ALERT_TOPIC)), "WorkstationAlertSub");
   alertSub.subscribe(
      [&](const RadarAlert &msg)
      {
         bool isFirst = !firstAlertSeen.exchange(true);
         GPINFO("{}[RadarAlert] alert_id={} severity={} component={} message={}",
                isFirst ? "[late-joiner snapshot] " : "",
                msg.alert_id(), static_cast<int>(msg.severity()), msg.component_id(), msg.message());
      });
   alertSub.start();

   GPINFO("RadarDDSWorkstation running. Press Ctrl+C to stop.");

   const std::vector<CommandType> commandCycle = {
      CommandType::CMD_START_SCAN, CommandType::CMD_SET_MODE, CommandType::CMD_STOP_SCAN};
   int32_t sequence = 0;

   while (isRunning.load())
   {
      Command cmd;
      cmd.header().sender_id("Workstation");
      cmd.header().timestamp_ns(nowNs());
      cmd.command_id("cmd-" + std::to_string(sequence));
      cmd.target_id("radar-1");
      cmd.type(commandCycle[static_cast<size_t>(sequence) % commandCycle.size()]);
      cmd.parameters("");
      cmd.sequence_number(sequence);

      commandPub.publish(cmd);
      GPINFO("[Command] sent command_id={} type={}", cmd.command_id(), static_cast<int>(cmd.type()));

      ++sequence;
      sleepResponsive(std::chrono::seconds(3));
   }

   GPINFO("RadarDDSWorkstation shutting down after {} commands, {} tracks ({} drops observed)",
          sequence, tracksReceived, trackDropsObserved);

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
