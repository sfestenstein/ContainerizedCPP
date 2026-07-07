#ifndef ZYREPUBLISHER_H
#define ZYREPUBLISHER_H

#include "ZyreNode.h"

#include <google/protobuf/message.h>

class ZyrePublisher : public ZyreNode
{
public:
    /**
     * @brief Construct a Zyre publisher.
     * @param name Namespace name for topic isolation
    * @param interfaceAddr Network interface to use (interface name or local IP).
    *        (default: "" lets Zyre auto-detect the interface)
     */
    explicit ZyrePublisher(const std::string &name,
                           const std::string &interfaceAddr = "");
    ~ZyrePublisher();

    // Publish a protobuf message to the specified topic
    // Returns true on success, false on failure
    bool publish(const std::string &topic,
                 const google::protobuf::Message &message);
};

#endif // ZYREPUBLISHER_H
