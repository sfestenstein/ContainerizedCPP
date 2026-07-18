#ifndef DDSCORE_IDISCOVERYSERVICE_H_
#define DDSCORE_IDISCOVERYSERVICE_H_

#include "DdsCore/DdsTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace DdsCore
{

/**
 * @brief Vendor-agnostic dynamic topic discovery.
 *
 * Reports which topics currently have at least one active publisher, and
 * the metadata (type name, QoS) needed to subscribe/publish to them.
 */
class IDiscoveryService
{
public:
   virtual ~IDiscoveryService() = default;

   /// List of currently-known topic names.
   [[nodiscard]] virtual std::vector<std::string> topicNames() const = 0;

   /// Look up discovery metadata for a single topic, if known.
   [[nodiscard]] virtual std::optional<DiscoveredTopic>
   lookup(const std::string &topic) const = 0;

   /// Register a callback invoked whenever the known-topic set changes.
   virtual void setTopicsChangedCallback(TopicsChangedCallback callback) = 0;
};

} // namespace DdsCore

#endif // DDSCORE_IDISCOVERYSERVICE_H_
