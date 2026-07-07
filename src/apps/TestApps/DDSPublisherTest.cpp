/**
 * @file DDSPublisherTest.cpp
 * @brief Test application for the DDS publisher.
 *
 * Publishes SensorReading and TrackUpdate samples on DDS topics.
 */

#include "CycloneDDS/CycloneDDSConfig.h"
#include "CycloneDDS/DDSPublisher.h"
#include "CommonUtils/GeneralLogger.h"

#include "SensorData.hpp"
#include "TrackData.hpp"

#include <chrono>
#include <csignal>
#include <random>
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
   logger.init("DDSPublisherTest");

   uint32_t domainId = 0;
   if (argc > 1)
   {
      domainId = static_cast<uint32_t>(std::stoul(argv[1]));
   }
   GPINFO("Using DDS domain ID: {}", domainId);

   CycloneDDS::CycloneDDSConfig defaults;
   defaults.defaultInitialize();
   const auto &config = defaults.config();

   // Create typed publishers — each gets its own TopicEntry
   CycloneDDS::DDSPublisher<dds_messages::SensorReading> sensorPub(
      domainId, config.getEntry(std::string(CycloneDDS::SENSOR_TOPIC)), "SensorPublisher");
   CycloneDDS::DDSPublisher<dds_messages::TrackUpdate> trackPub(
      domainId, config.getEntry(std::string(CycloneDDS::TRACK_TOPIC)), "TrackPublisher");

   std::random_device rd;
   std::mt19937 gen(rd());
   std::uniform_real_distribution<> tempDist(20.0, 35.0);
   std::uniform_real_distribution<> latDist(37.0, 38.0);
   std::uniform_real_distribution<> lonDist(-123.0, -122.0);
   std::uniform_real_distribution<> headingDist(0.0, 360.0);
   std::uniform_real_distribution<> speedDist(0.0, 100.0);

   int sequence = 0;

   while (isRunning.load())
   {
      auto now = std::chrono::system_clock::now();
      auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
         now.time_since_epoch()).count();

      // Publish a sensor reading
      dds_messages::SensorReading sensor;
      sensor.sensor_id("sensor-001");
      sensor.sensor_name("Temperature Sensor");
      sensor.value(tempDist(gen));
      sensor.unit("celsius");
      sensor.timestamp_ms(epochMs);
      sensor.quality(95);
      sensor.status(dds_messages::SensorStatus::SENSOR_ONLINE);
      sensor.latitude(37.7749);
      sensor.longitude(-122.4194);
      sensor.altitude(10.0);

      sensorPub.publish(sensor);

      // Publish a track update
      dds_messages::TrackUpdate track;
      track.track_id("track-alpha");
      track.track_name("Aircraft Alpha");
      track.latitude(latDist(gen));
      track.longitude(lonDist(gen));
      track.altitude(10000.0);
      track.heading(headingDist(gen));
      track.speed(speedDist(gen));
      track.classification(dds_messages::TrackClassification::TRACK_FRIENDLY);
      track.timestamp_ms(epochMs);
      track.update_number(sequence);
      track.confidence(0.95);

      trackPub.publish(track);

      ++sequence;
      if (sequence % 100 == 0)
      {
         GPINFO("Published {} sensor + track samples", sequence);
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }

   GPINFO("DDSPublisherTest shutting down after {} messages", sequence);
   return 0;
}
