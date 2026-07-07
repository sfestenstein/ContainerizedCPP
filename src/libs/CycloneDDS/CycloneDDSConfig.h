#ifndef CYCLONEDDSCONFIG_H_
#define CYCLONEDDSCONFIG_H_

// Project headers
#include "CycloneDDS/DDSTopicConfig.h"

#include <string_view>

namespace CycloneDDS
{

// Canonical topic name constants — use these instead of string literals.
inline constexpr std::string_view SENSOR_TOPIC = "SensorTopic";
inline constexpr std::string_view TRACK_TOPIC  = "TrackTopic";

/**
 * @brief Provides a pre-built DDSTopicConfig with sensible default QoS
 *        for every project topic.
 *
 * Centralises topic names and QoS policies so that publishers and
 * subscribers always agree.  Call defaultInitialize() once and share
 * the returned config across all participants.
 *
 * Usage:
 * @code
 *    CycloneDDS::CycloneDDSConfig defaults;
 *    defaults.defaultInitialize();
 *    const auto &config = defaults.config();
 *
 *    CycloneDDS::DDSPublisher<dds_messages::SensorReading>  pub(0, config.getEntry("SensorTopic"));
 *    CycloneDDS::DDSSubscriber<dds_messages::SensorReading> sub(0, config.getEntry("SensorTopic"));
 * @endcode
 */
class CycloneDDSConfig
{
public:
   /**
    * @brief Populate the internal DDSTopicConfig with default topics/QoS.
    *
    * Current defaults:
    *   - SensorTopic : Reliable (1 s timeout), KeepLast(10)
    *   - TrackTopic  : BestEffort, KeepLast(1)
    */
   void defaultInitialize();

   /**
    * @brief Access the initialised DDSTopicConfig.
    */
   [[nodiscard]] const DDSTopicConfig &config() const;

private:
   DDSTopicConfig _config{{}};
};

} // namespace CycloneDDS

#endif // CYCLONEDDSCONFIG_H_
