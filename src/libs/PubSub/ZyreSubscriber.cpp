#include "ZyreSubscriber.h"
#include "GeneralLogger.h"

#include <cstring>
#include <iostream>

ZyreSubscriber::ZyreSubscriber(const std::string &name,
                               const std::string &interfaceAddr) :
    ZyreNode(name, interfaceAddr)
{
    if (!start()) 
    {
        GPERROR("Failed to start subscriber node");
        return;
    }

    // Spawn background thread to handle incoming messages
    _receiveThread = std::thread(&ZyreSubscriber::receiveLoop, this);
}

ZyreSubscriber::~ZyreSubscriber() 
{
    stop();
    if (_receiveThread.joinable())
    {
        _receiveThread.join();
    }
}

void ZyreSubscriber::subscribe(const std::string &topic, MessageHandler handler)
{
    // Create namespaced group name
    const std::string namespacedTopic = _nodeName + "/" + topic;

    {
        const std::lock_guard<std::mutex> lock(_handlersMutex);
        _handlers[namespacedTopic] = std::move(handler);
    }

    // Join the zyre group for this topic if node is running
    if (_node)
    {
        zyre_join(_node, namespacedTopic.c_str());
    }
}

void ZyreSubscriber::receiveLoop() 
{
    while (true) 
    {
        zyre_event_t *event = zyre_event_new(_node);
        if (!event)
        {
            // Node likely shutting down
            break;
        }
        const char *type = zyre_event_type(event);

        // Check for STOP event - indicates zyre_stop() was called
        if (type && strcmp(type, "STOP") == 0)
        {
            zyre_event_destroy(&event);
            break;
        }

        if (type && strcmp(type, "SHOUT") == 0) 
        {
            const char *group = zyre_event_group(event);
            zmsg_t *zmsg = zyre_event_get_msg(event);
            
            if (zmsg && group) 
            {
                const std::string topic(group);
                
                // Get raw binary data from the message
                zframe_t *frame = zmsg_first(zmsg);
                if (frame)
                {
                    const std::string data(reinterpret_cast<const char*>(zframe_data(frame)), 
                                     zframe_size(frame));
                    
                    // Find and invoke the handler
                    MessageHandler handler;
                    {
                        const std::lock_guard<std::mutex> lock(_handlersMutex);
                        auto it = _handlers.find(topic);
                        if (it != _handlers.end())
                        {
                            handler = it->second;
                        }
                    }
                    
                    if (handler)
                    {
                        handler(topic, data);
                    }
                }
                zmsg_destroy(&zmsg);
            }
        }

        if (type && strcmp(type, "ENTER") == 0)
        {
            const char *peerName = zyre_event_peer_name(event);
            const char *peerUuid = zyre_event_peer_uuid(event);
            GPINFO("Peer ENTER: name={} uuid={}", peerName ? peerName : "(null)",
                   peerUuid ? peerUuid : "(null)");
        }

        if (type && strcmp(type, "EXIT") == 0)
        {
            const char *peerName = zyre_event_peer_name(event);
            const char *peerUuid = zyre_event_peer_uuid(event);
            GPINFO("Peer EXIT: name={} uuid={}", peerName ? peerName : "(null)",
                   peerUuid ? peerUuid : "(null)");
        }

        zyre_event_destroy(&event);
    }
}
