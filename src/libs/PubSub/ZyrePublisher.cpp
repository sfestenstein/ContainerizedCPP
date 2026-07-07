#include "ZyrePublisher.h"

#include <iostream>

#include <zyre.h>

#include "GeneralLogger.h"

ZyrePublisher::ZyrePublisher(const std::string &name,
                             const std::string &interfaceAddr) :
    ZyreNode(name, interfaceAddr)
{
}

ZyrePublisher::~ZyrePublisher()
{
    _isRunning.store(false);
    stop();
}

bool ZyrePublisher::publish(const std::string &topic, const google::protobuf::Message &message)
{
    if (!_node || !_isRunning.load()) 
    {
        GPERROR("Publisher not running");
        return false;
    }

    // Serialize the protobuf message
    std::string serialized;
    if (!message.SerializeToString(&serialized)) 
    {
        GPERROR("Failed to serialize protobuf message");
        return false;
    }

    // Create namespaced group name
    const std::string namespacedTopic = _nodeName + "/" + topic;

    // Create zmsg and add the serialized data
    zmsg_t *zmsg = zmsg_new();
    zmsg_addmem(zmsg, serialized.data(), serialized.size());

    if (zyre_shout(_node, namespacedTopic.c_str(), &zmsg) != 0) 
    {
        GPERROR("Failed to shout on topic: " + topic);
        if (zmsg) zmsg_destroy(&zmsg);
        return false;
    }

    return true;
}