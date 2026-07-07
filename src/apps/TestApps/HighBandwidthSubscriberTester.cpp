#include "HighBandwidthSubscriber.h"
#include <chrono>
#include <csignal>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

#include "GeneralLogger.h"

#include <sensor_reading.pb.h>
#include <sensor_data_batch.pb.h>
#include <command.pb.h>
#include <application_config.pb.h>

namespace
{

std::string formatFixed(const double value, const int precision)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(precision) << value;
    return os.str();
}

} // namespace

// NOLINTNEXTLINE
int main(int argc, char* argv[]) 
{
    // Initialize logger
    CommonUtils::GeneralLogger logger;
    logger.init("HighBandwidthSubscriber");

    // Optional: specify local interface IP via command line
    // Usage: ./HighBandwidthSubscriber [interface_ip]
    std::string interfaceAddr;
    if (argc > 1)
    {
        interfaceAddr = argv[1];
        GPINFO("Using interface address: {}", interfaceAddr);
    }

    // High-bandwidth UDP multicast subscriber (namespace must match publisher)
    HighBandwidthSubscriber sub("TestHb", "239.192.1.1", 5670, 1000, interfaceAddr);

    // Subscribe to SensorReading messages
    sub.subscribe("SensorReading", [](const std::string &topic, const std::string &data)
    {
        messages::SensorReadingMsg msg;
        if (msg.ParseFromString(data)) 
        {
             const auto valueText = formatFixed(msg.value(), 2);
                 GPINFO("Received SensorReadingMsg on {}: {} = {} {} (quality: {}, status: {})",
                 topic, msg.sensor_name(), valueText, msg.unit(), 
                   msg.quality(), static_cast<int>(msg.status()));
            
            if (msg.has_location())
            {
              const auto latText = formatFixed(msg.location().latitude(), 4);
              const auto lonText = formatFixed(msg.location().longitude(), 4);
              const auto altText = formatFixed(msg.location().altitude(), 1);
              GPDEBUG("  Location: lat={}, lon={}, alt={}m",
                   latText, lonText, altText);
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
                const auto readingValueText = formatFixed(reading.value(), 2);
                GPDEBUG("  [{}] {} = {} {}", 
                        reading.sensor_id(), reading.sensor_name(), 
                    readingValueText, reading.unit());
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

    // Start receiving
    if (!sub.start())
    {
        GPERROR("Failed to start subscriber");
        return 1;
    }

    GPINFO("Subscriber running. Press Ctrl+C to exit.");

    // Keep the main thread alive
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    sub.stop();
    GPINFO("Subscriber stopped.");
    return 0;
}
