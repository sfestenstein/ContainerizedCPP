#ifndef GRPCCHATLOGGER_CHATMESSAGESTORE_H_
#define GRPCCHATLOGGER_CHATMESSAGESTORE_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace GrpcChatLogger
{

struct StoredMessage
{
   int64_t id;
   std::string text;
   int64_t timestampMs;
};

/**
 * @brief Thread-safe bounded ring buffer of the most recently reported chat
 * messages. Oldest messages are evicted once the buffer exceeds capacity.
 */
class ChatMessageStore
{
public:
   explicit ChatMessageStore(std::size_t capacity = 100);

   /**
    * @brief Store a new message, assigning it the next sequential id and the
    * current wall-clock timestamp.
    */
   StoredMessage push(std::string text);

   /**
    * @brief The most recently stored message, or std::nullopt if none has
    * been reported yet.
    */
   std::optional<StoredMessage> latest() const;

private:
   std::size_t _capacity;
   mutable std::mutex _mutex;
   std::deque<StoredMessage> _messages;
   int64_t _nextId = 1;
};

} // namespace GrpcChatLogger

#endif // GRPCCHATLOGGER_CHATMESSAGESTORE_H_
