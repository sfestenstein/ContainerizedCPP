#include "ZyreNode.h"

#include <zyre.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <optional>

#include "GeneralLogger.h"

namespace
{
std::optional<std::string> resolveInterfaceNameFromIp(const std::string &ip)
{
    ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0 || !ifaddr)
    {
        return std::nullopt;
    }

    std::optional<std::string> resolved;
    for (auto *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || !ifa->ifa_name)
        {
            continue;
        }

        const int family = ifa->ifa_addr->sa_family;
        if (family != AF_INET)
        {
            continue;
        }

        std::array<char, INET_ADDRSTRLEN> addrBuf = {0};
        const auto *sin = reinterpret_cast<const sockaddr_in *>(ifa->ifa_addr);
        if (!inet_ntop(AF_INET, &(sin->sin_addr), addrBuf.data(), addrBuf.size()))
        {
            continue;
        }

        if (ip == addrBuf.data())
        {
            resolved = std::string(ifa->ifa_name);
            break;
        }
    }

    freeifaddrs(ifaddr);
    return resolved;
}
}

ZyreNode::ZyreNode(const std::string &name,
                   const std::string &interfaceAddr) : 
    _node(zyre_new(nullptr)),  // Use random UUID for actual node name
    _nodeName(name)
{
    // Bind Zyre beacons and traffic to a specific network interface
    if (_node && !interfaceAddr.empty())
    {
        std::string interfaceName = interfaceAddr;

        // Zyre expects an interface name (e.g. "eth0", "en0").
        // The test apps historically pass an IP; resolve IP->interface name when possible.
        if (const auto resolved = resolveInterfaceNameFromIp(interfaceAddr))
        {
            interfaceName = *resolved;
            GPINFO("Resolved interface IP {} -> {}", interfaceAddr, interfaceName);
        }
        else
        {
            GPINFO("Using interface selector as provided: {}", interfaceName);
        }

        zyre_set_interface(_node, interfaceName.c_str());
        GPINFO("Zyre node '{}' bound to interface {}", name, interfaceName);
    }
}

ZyreNode::~ZyreNode() 
{
    stop();

    if (_node) 
    {
        zyre_destroy(&_node);
        _node = nullptr;
    }
}

bool ZyreNode::start() 
{
    if (!_node) return false;

    // Reset stop state in case of restart
    {
        const std::lock_guard<std::mutex> lock(_terminateMutex);
        _stopRequested = false;
    }
    _isRunning.store(true);

    // start zyre
    return zyre_start(_node) == 0;
}

void ZyreNode::stop() 
{
    {
        const std::lock_guard<std::mutex> lock(_terminateMutex);
        if (_stopRequested) return;  // Already stopped
        _stopRequested = true;
    }
    _terminateCV.notify_all();

    _isRunning.store(false);

    if (_node) 
    {
        zyre_stop(_node);
    }
}
