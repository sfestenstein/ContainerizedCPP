#include "ZyreSubscriber.h"
#include "GeneralLogger.h"

#include <chrono>
#include <csignal>
#include <string>
#include <thread>

#include <czmq.h>

#include <sensor_reading.pb.h>
#include <sensor_data_batch.pb.h>
#include <command.pb.h>
#include <application_config.pb.h>

// NOLINTNEXTLINE
int main(int argc, char* argv[]) 
{
    // Initialize logger
    CommonUtils::GeneralLogger logger;
    logger.init("ZyreSubscriber");

    // Disable CZMQ's signal handling so we can use our own
    zsys_handler_set(nullptr);

    // Optional: specify local interface IP via command line
    // Usage: ./ZyreSubscriber [interface_name|interface_ip]
    std::string interfaceAddr;
    if (argc > 1)
    {
        interfaceAddr = argv[1];
        GPINFO("Using interface address: {}", interfaceAddr);
    }

    ZyreSubscriber sub("TestZyre", interfaceAddr);

    // Subscribe to SensorReading messages
    sub.subscribe("SensorReading", [](const std::string &topic, const std::string &data)
    {
        messages::SensorReadingMsg msg;
        if (msg.ParseFromString(data)) 
        {
            GPINFO("Received SensorReadingMsg on {}: {} = {:.2f} {} (quality: {}, status: {})",
                   topic, msg.sensor_name(), msg.value(), msg.unit(), 
                   msg.quality(), static_cast<int>(msg.status()));
            
            if (msg.has_location())
            {
                GPDEBUG("  Location: lat={:.4f}, lon={:.4f}, alt={:.1f}m",
                        msg.location().latitude(), msg.location().longitude(), 
                        msg.location().altitude());
            }
            
            if (msg.metadata_size() > 0)
            {
                for (const auto& [key, value] : msg.metadata())
                {
                    GPDEBUG("  Metadata: {} = {}", key, value);
                }
            }
        } 
        else 
        {
            GPERROR("Failed to parse SensorReadingMsg");
        }
    });

    // Subscribe to SensorDataBatch messages
    sub.subscribe("SensorDataBatch", [](const std::string &topic, const std::string &data)
    {
        messages::SensorDataBatchMsg msg;
        if (msg.ParseFromString(data)) 
        {
            GPINFO("Received SensorDataBatchMsg on {}: batch_id={}, source={}, readings={}",
                   topic, msg.batch_id(), msg.source_system(), msg.readings_size());
            
            for (int i = 0; i < msg.readings_size(); ++i)
            {
                const auto& reading = msg.readings(i);
                GPDEBUG("  [{}] {} = {:.2f} {}", 
                        reading.sensor_id(), reading.sensor_name(), 
                        reading.value(), reading.unit());
            }
        } 
        else 
        {
            GPERROR("Failed to parse SensorDataBatchMsg");
        }
    });

    // Subscribe to Command messages
    sub.subscribe("Command", [](const std::string &topic, const std::string &data)
    {
        messages::CommandMsg msg;
        if (msg.ParseFromString(data)) 
        {
            GPINFO("Received CommandMsg on {}: id={}, type={}, target={}, issuer={}",
                   topic, msg.command_id(), static_cast<int>(msg.type()), 
                   msg.target(), msg.issuer());
            
            if (msg.has_query_params())
            {
                const auto& params = msg.query_params();
                GPDEBUG("  Query type: {}, fields: {}", 
                        params.query_fields(0), params.max_results());
            }
        } 
        else 
        {
            GPERROR("Failed to parse CommandMsg");
        }
    });

    GPINFO("Subscriber running. Press Ctrl+C to exit.");

    // Keep the main thread alive - subscriber runs in background
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
