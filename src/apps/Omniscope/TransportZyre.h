#ifndef TRANSPORTZYRE_H_
#define TRANSPORTZYRE_H_

#include "ITransport.h"

#include <memory>
#include <string>

namespace Omniscope
{

/**
 * @brief Zyre transport implementation for protobuf messages.
 *
 * Subscribes/publishes on Zyre topics listed in ProtoTopicConfig.
 * Binary protobuf payloads are converted to/from JSON via
 * ProtoJsonDispatch so the rest of Omniscope stays transport-agnostic.
 *
 * Uses pImpl to keep Zyre/protobuf headers out of the public interface.
 */
class TransportZyre : public ITransport
{
public:
   /**
    * @param nodeName  Zyre namespace for topic isolation
    *                  (e.g. "Omniscope").  Topics will appear on the
    *                  network as "<nodeName>/<TopicName>".
    */
   explicit TransportZyre(const std::string &nodeName);
   ~TransportZyre() override;

   TransportZyre(const TransportZyre &) = delete;
   TransportZyre &operator=(const TransportZyre &) = delete;

   [[nodiscard]] std::string name() const override;
   [[nodiscard]] std::vector<std::string> topicNames() const override;

   void subscribe(const std::string &topic, MessageCallback callback) override;
   void unsubscribe(const std::string &topic) override;
   [[nodiscard]] bool isSubscribed(const std::string &topic) const override;

   void publishFromJson(const std::string &topic,
                        const std::string &jsonData) override;

private:
   struct Impl;
   std::unique_ptr<Impl> _impl;
};

} // namespace Omniscope

#endif // TRANSPORTZYRE_H_
