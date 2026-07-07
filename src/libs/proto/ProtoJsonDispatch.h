#ifndef PROTOJSONDISPATCH_H_
#define PROTOJSONDISPATCH_H_

#include "ProtoTopicConfig.h"

#include "application_config.pb.h"
#include "command.pb.h"
#include "command_response.pb.h"
#include "sensor_data_batch.pb.h"
#include "sensor_reading.pb.h"

#include <google/protobuf/util/json_util.h>

#include <functional>
#include <memory>
#include <string>

namespace ProtoMessages
{

/**
 * @brief Runtime dispatch between TopicId and concrete protobuf types.
 *
 * Provides:
 *  - Binary-to-JSON conversion (subscribe path).
 *  - JSON-to-binary conversion (publish/playback path).
 *
 * Transport-agnostic: usable from any ITransport implementation that
 * carries protobuf payloads.
 */
class ProtoJsonDispatch
{
public:
   /**
    * @brief Deserialize a binary protobuf payload into a JSON string.
    * @param id     TopicId that determines the concrete message type.
    * @param binary Raw bytes produced by Message::SerializeToString().
    * @param json   Output — JSON representation of the message.
    * @return true on success; false if the TopicId is unknown or
    *         deserialization / JSON conversion fails.
    */
   [[nodiscard]] static bool toJson(TopicId id,
                                    const std::string &binary,
                                    std::string &json)
   {
      auto msg = createMessage(id);
      if (!msg) return false;

      if (!msg->ParseFromString(binary)) return false;

      google::protobuf::util::JsonPrintOptions opts;
      opts.preserve_proto_field_names = true;

      auto status = google::protobuf::util::MessageToJsonString(
         *msg, &json, opts);
      return status.ok();
   }

   /**
    * @brief Convert a JSON string to a serialized protobuf binary.
    * @param id     TopicId that determines the concrete message type.
    * @param json   JSON representation of the message.
    * @param binary Output — raw bytes suitable for publishing.
    * @return true on success.
    */
   [[nodiscard]] static bool fromJson(TopicId id,
                                      const std::string &json,
                                      std::string &binary)
   {
      auto msg = fromJsonToMessage(id, json);
      if (!msg) return false;
      return msg->SerializeToString(&binary);
   }

   /**
    * @brief Convert a JSON string to a concrete protobuf Message.
    * @param id   TopicId that determines the concrete message type.
    * @param json JSON representation of the message.
    * @return Populated message, or nullptr on failure.
    */
   [[nodiscard]] static std::unique_ptr<google::protobuf::Message>
   fromJsonToMessage(TopicId id, const std::string &json)
   {
      auto msg = createMessage(id);
      if (!msg) return nullptr;

      auto status = google::protobuf::util::JsonStringToMessage(json, msg.get());
      if (!status.ok()) return nullptr;
      return msg;
   }

private:
   /// Factory: returns a default-constructed message for the given topic.
   [[nodiscard]] static std::unique_ptr<google::protobuf::Message>
   createMessage(TopicId id)
   {
      switch (id)
      {
         case TopicId::SensorReading:
            return std::make_unique<messages::SensorReadingMsg>();
         case TopicId::SensorDataBatch:
            return std::make_unique<messages::SensorDataBatchMsg>();
         case TopicId::Command:
            return std::make_unique<messages::CommandMsg>();
         case TopicId::CommandResponse:
            return std::make_unique<messages::CommandResponseMsg>();
         case TopicId::ApplicationConfig:
            return std::make_unique<messages::ApplicationConfigMsg>();
         default:
            return nullptr;
      }
   }
};

} // namespace ProtoMessages

#endif // PROTOJSONDISPATCH_H_
