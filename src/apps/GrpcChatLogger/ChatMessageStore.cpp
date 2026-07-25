#include "ChatMessageStore.h"

#include <chrono>

namespace GrpcChatLogger
{

ChatMessageStore::ChatMessageStore(std::size_t capacity) : _capacity(capacity)
{
}

StoredMessage ChatMessageStore::push(std::string text)
{
   auto timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

   std::lock_guard<std::mutex> lock(_mutex);
   StoredMessage message{_nextId++, std::move(text), timestampMs};
   _messages.push_back(message);
   if (_messages.size() > _capacity)
   {
      _messages.pop_front();
   }
   return message;
}

std::optional<StoredMessage> ChatMessageStore::latest() const
{
   std::lock_guard<std::mutex> lock(_mutex);
   if (_messages.empty())
   {
      return std::nullopt;
   }
   return _messages.back();
}

} // namespace GrpcChatLogger
