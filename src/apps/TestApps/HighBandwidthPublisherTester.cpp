#include "HighBandwidthPublisher.h"
#include <chrono>
#include <csignal>
#include <string>
#include <thread>
#include <random>

#include "GeneralLogger.h"

#include <sensor_reading.pb.h>
#include <sensor_data_batch.pb.h>
#include <command.pb.h>
#include <application_config.pb.h>

// NOLINTNEXTLINE
int main(int argc, char* argv[]) 
{
    // Initialize logger
    CommonUtils::GeneralLogger logger;
    logger.init("HighBandwidthPublisherTester");

    // Optional: specify local interface IP via command line
    // Usage: ./HighBandwidthPublisher [interface_ip]
    std::string interfaceAddr;
    if (argc > 1)
    {
        interfaceAddr = argv[1];
        GPINFO("Using interface address: {}", interfaceAddr);
    }

    // High-bandwidth UDP multicast publisher
    HighBandwidthPublisher pub("TestHb", "239.192.1.1", 5670, 1400, interfaceAddr);

    // Thread-safe random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> tempDist(0.0, 10.0);
    std::uniform_real_distribution<> batchDist(0.0, 5.0);

    int messageCount = 0;

    while (true)
    {
    auto now = std::chrono::system_clock::now();
        auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        // Create and publish a SensorReadingMsg message
        messages::SensorReadingMsg sensorReading;
        sensorReading.set_sensor_id("sensor-001");
        sensorReading.set_sensor_name("Temperature Sensor");
        sensorReading.set_value(22.5 + tempDist(gen));  // Random temperature 22.5-32.5
        sensorReading.set_unit("celsius");
        sensorReading.set_timestamp_ms(epochMs);
        sensorReading.set_quality(95);
        sensorReading.set_status(messages::SENSOR_STATUS_ONLINE);
        
        auto* location = sensorReading.mutable_location();
        location->set_latitude(37.7749);
        location->set_longitude(-122.4194);
        location->set_altitude(10.0);
        
        (*sensorReading.mutable_metadata())["building"] = "HQ";
        (*sensorReading.mutable_metadata())["floor"] = "3";

        pub.publish("SensorReading", sensorReading);
        GPINFO("Published SensorReading: {} = {:.1f} {}", 
               sensorReading.sensor_name(), sensorReading.value(), sensorReading.unit());

        // Create and publish a SensorDataBatchMsg message
        messages::SensorDataBatchMsg batch;
        batch.set_batch_id("batch-" + std::to_string(messageCount));
        batch.set_created_at_ms(epochMs);
        batch.set_source_system("TestPublisher");

        // Add multiple readings to the batch
        for (int i = 0; i < 3; ++i)
        {
            auto* reading = batch.add_readings();
            reading->set_sensor_id("sensor-00" + std::to_string(i + 1));
            reading->set_sensor_name("Sensor " + std::to_string(i + 1));
            reading->set_value(20.0 + (i * 5.0) + batchDist(gen));
            reading->set_unit("celsius");
            reading->set_timestamp_ms(epochMs);
            reading->set_quality(90 + i);
            reading->set_status(messages::SENSOR_STATUS_ONLINE);
        }

        pub.publish("SensorDataBatch", batch);
        GPINFO("Published SensorDataBatch: {} with {} readings", 
               batch.batch_id(), batch.readings_size());

        // Create and publish a Command message every 5th iteration
        if (messageCount % 5 == 0)
        {
            messages::CommandMsg command;
            command.set_command_id("cmd-" + std::to_string(messageCount));
            command.set_type(messages::COMMAND_TYPE_QUERY);
            command.set_target("sensor-001");
            command.set_issued_at_ms(epochMs);
            command.set_timeout_ms(5000);
            command.set_priority(1);
            command.set_issuer("TestPublisher");

            auto* queryParams = command.mutable_query_params();
            queryParams->add_query_fields("value");
            queryParams->mutable_filters()->insert({"status", "online"});
            queryParams->mutable_filters()->insert({"BitStatus", "Degraded"});
            queryParams->set_max_results(10);

            pub.publish("Command", command);
            GPINFO("Published Command: {} type={}", 
                   command.command_id(), static_cast<int>(command.type()));
        }

        ++messageCount;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    GPINFO("Shutting down...");
    return 0;
}
