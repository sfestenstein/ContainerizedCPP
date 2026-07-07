/**
 * @file DDSSubscriberTest.cpp
 * @brief Test application for the DDS subscriber.
 *
 * Subscribes to SensorReading and TrackUpdate DDS topics.
 */

#include "CycloneDDS/CycloneDDSConfig.h"
#include "CycloneDDS/DDSSubscriber.h"
#include "CommonUtils/GeneralLogger.h"

#include "SensorData.hpp"
#include "TrackData.hpp"

#include <chrono>
#include <csignal>
#include <thread>

static std::atomic<bool> isRunning{true};

void signalHandler(int)
{
   isRunning.store(false);
}
// NOLINTNEXTLINE
int main(int argc, char *argv[])
{
   (void)std::signal(SIGINT, signalHandler);
   (void)std::signal(SIGTERM, signalHandler);

   CommonUtils::GeneralLogger logger;
   logger.init("DDSSubscriberTest");

   uint32_t domainId = 0;
   if (argc > 1)
   {
      domainId = static_cast<uint32_t>(std::stoul(argv[1]));
   }
   GPINFO("Using DDS domain ID: {}", domainId);

   CycloneDDS::CycloneDDSConfig defaults;
   defaults.defaultInitialize();
   const auto &config = defaults.config();

   // Sensor subscriber
   CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sensorSub(
      domainId, config.getEntry(std::string(CycloneDDS::SENSOR_TOPIC)), "SensorSubscriber");
   sensorSub.subscribe(
      [](const dds_messages::SensorReading &msg)
      {
         GPINFO("Sensor [{}]: value={:.2f} {} (quality={})",
            msg.sensor_id(), msg.value(), msg.unit(), msg.quality());
      });

   // Track subscriber
   CycloneDDS::DDSSubscriber<dds_messages::TrackUpdate> trackSub(
      domainId, config.getEntry(std::string(CycloneDDS::TRACK_TOPIC)), "TrackSubscriber");
   trackSub.subscribe(
      [](const dds_messages::TrackUpdate &msg)
      {
         GPINFO("Track [{}]: lat={:.4f} lon={:.4f} alt={:.0f} hdg={:.1f} spd={:.1f}",
            msg.track_id(), msg.latitude(), msg.longitude(),
            msg.altitude(), msg.heading(), msg.speed());
      });

   sensorSub.start();
   trackSub.start();

   GPINFO("DDSSubscriberTest running. Press Ctrl+C to stop.");

   while (isRunning.load())
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }

   sensorSub.stop();
   trackSub.stop();

   GPINFO("DDSSubscriberTest shutting down.");
   return 0;
}
