#ifndef RADARTOPICS_H_
#define RADARTOPICS_H_

#include "CycloneDDS/DDSTopicConfig.h"

#include <string_view>

namespace RadarDemo
{

// Canonical topic name constants — use these instead of string literals.
inline constexpr std::string_view COMMAND_TOPIC = "RadarCommand";
inline constexpr std::string_view COMMAND_STATUS_TOPIC = "RadarCommandStatus";
inline constexpr std::string_view RADAR_TRACK_TOPIC = "RadarTrack";
inline constexpr std::string_view COMPONENT_STATUS_TOPIC = "RadarComponentStatus";
inline constexpr std::string_view RADAR_ALERT_TOPIC = "RadarAlert";

/**
 * @brief Builds the CycloneDDS::DDSTopicConfig for all 5 RadarDDSDemo topics.
 *
 * Every topic sets reliability, durability, and history explicitly on BOTH
 * the writer and reader QoS so that RxO (Request-vs-Offered) compatibility
 * is guaranteed rather than relying on either side's defaults.
 *
 * QoS profile per topic (see RadarDDSDemo's plan/README for the reasoning):
 *   - RadarCommand         : Reliable,    Volatile,      KeepLast(10)
 *   - RadarCommandStatus   : Reliable,    Volatile,      KeepLast(10)
 *   - RadarTrack           : Best Effort, Volatile,      KeepLast(1)
 *   - RadarComponentStatus : Reliable,    TransientLocal, KeepLast(1)
 *   - RadarAlert           : Reliable,    TransientLocal, KeepAll (max 50/instance)
 */
class RadarTopics
{
public:
   RadarTopics();

   [[nodiscard]] const CycloneDDS::DDSTopicConfig &config() const;

private:
   CycloneDDS::DDSTopicConfig _config;
};

} // namespace RadarDemo

#endif // RADARTOPICS_H_
