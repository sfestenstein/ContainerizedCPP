#ifndef PROTOTOPICCONFIG_H_
#define PROTOTOPICCONFIG_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ProtoMessages
{

/**
 * @brief Identifies every publishable protobuf topic in the system.
 *
 * Each enumerator corresponds to exactly one proto Msg type.
 * COUNT must always be the last entry.
 */
enum class TopicId : std::uint8_t
{
   SensorReading,
   SensorDataBatch,
   Command,
   CommandResponse,
   ApplicationConfig,
   COUNT  // sentinel — must remain last
};

/**
 * @brief Compile-time mapping between a TopicId and its canonical
 *        topic-name string.
 */
struct TopicInfo
{
   TopicId    id;
   std::string_view name;
};

/// Authoritative registry — one entry per TopicId value.
inline constexpr std::array<TopicInfo,
   static_cast<std::size_t>(TopicId::COUNT)> TOPIC_REGISTRY
{{
   { .id=TopicId::SensorReading,     .name="SensorReading"     },
   { .id=TopicId::SensorDataBatch,   .name="SensorDataBatch"   },
   { .id=TopicId::Command,           .name="Command"           },
   { .id=TopicId::CommandResponse,   .name="CommandResponse"    },
   { .id=TopicId::ApplicationConfig, .name="ApplicationConfig"  },
}};

// ── compile-time validations ────────────────────────────────────────────

/// Every TopicId enumerator must have exactly one registry entry.
static_assert(TOPIC_REGISTRY.size() ==
              static_cast<std::size_t>(TopicId::COUNT),
              "TOPIC_REGISTRY size must match TopicId::COUNT");

/**
 * @brief Return the canonical topic-name string for a TopicId.
 *
 * Usable at compile time:
 * @code
 *    static_assert(topicName(TopicId::SensorReading) == "SensorReading");
 * @endcode
 */
[[nodiscard]] constexpr std::string_view topicName(TopicId id)
{
   for (const auto &entry : TOPIC_REGISTRY)
   {
      if (entry.id == id)
      {
         return entry.name;
      }
   }
   // Unreachable for valid TopicId values — caught by static_assert above.
   return {};
}

/**
 * @brief Look up a TopicId from a topic-name string.
 * @return The matching TopicId, or std::nullopt if not found.
 *
 * Usable at compile time:
 * @code
 *    static_assert(topicId("Command") == TopicId::Command);
 * @endcode
 */
[[nodiscard]] constexpr std::optional<TopicId> topicId(std::string_view name)
{
   for (const auto &entry : TOPIC_REGISTRY)
   {
      if (entry.name == name)
      {
         return entry.id;
      }
   }
   return std::nullopt;
}

/**
 * @brief Collect all registered topic names (runtime helper).
 */
[[nodiscard]] inline std::vector<std::string> allTopicNames()
{
   std::vector<std::string> names;
   names.reserve(TOPIC_REGISTRY.size());
   for (const auto &entry : TOPIC_REGISTRY)
   {
      names.emplace_back(entry.name);
   }
   return names;
}

// ── compile-time self-tests ─────────────────────────────────────────────

static_assert(topicName(TopicId::SensorReading) == "SensorReading");
static_assert(topicName(TopicId::Command)       == "Command");
static_assert(topicId("SensorReading")          == TopicId::SensorReading);
static_assert(topicId("Command")                == TopicId::Command);
static_assert(!topicId("NonExistent").has_value());

} // namespace ProtoMessages

#endif // PROTOTOPICCONFIG_H_
