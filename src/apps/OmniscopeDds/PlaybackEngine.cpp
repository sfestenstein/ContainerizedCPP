#include "PlaybackEngine.h"

#include "CommonUtils/GeneralLogger.h"

#include <crow/json.h>

#include <algorithm>
#include <chrono>
#include <sstream>

namespace Omniscope
{

PlaybackEngine::PlaybackEngine(PublishCallback publish)
   : _publish(std::move(publish))
{
}

PlaybackEngine::~PlaybackEngine()
{
   stop();
}

size_t PlaybackEngine::loadRecording(const std::string &body)
{
   stop();

   std::lock_guard lock(_linesMutex);
   _lines.clear();

   std::istringstream stream(body);
   std::string line;
   while (std::getline(stream, line))
   {
      if (!line.empty()) _lines.push_back(std::move(line));
   }

   GPINFO("PlaybackEngine: loaded {} messages", _lines.size());
   return _lines.size();
}

bool PlaybackEngine::hasRecording() const
{
   std::lock_guard lock(_linesMutex);
   return !_lines.empty();
}

size_t PlaybackEngine::messageCount() const
{
   std::lock_guard lock(_linesMutex);
   return _lines.size();
}

void PlaybackEngine::start(ProgressCallback onProgress,
                           CompletionCallback onComplete)
{
   stop();

   // Snapshot the lines so we don't hold _linesMutex during playback
   std::vector<std::string> snapshot;
   {
      std::lock_guard lock(_linesMutex);
      snapshot = _lines;
   }

   if (snapshot.empty())
   {
      GPWARN("PlaybackEngine: no recording loaded");
      if (onComplete) onComplete(false);
      return;
   }

   _stopFlag.store(false);
   _playing.store(true);

   _thread = std::thread([this,
                          lines = std::move(snapshot),
                          onProgress = std::move(onProgress),
                          onComplete = std::move(onComplete)]()
   {
      auto firstRec = crow::json::load(lines[0]);
      int64_t baseTime = firstRec ? firstRec["timestamp_ms"].i() : 0;
      int64_t prevOffset = 0;
      size_t total = lines.size();

      GPINFO("PlaybackEngine: playback started, {} messages", total);

      for (size_t i = 0; i < total && !_stopFlag.load(); ++i)
      {
         auto rec = crow::json::load(lines[i]);
         if (!rec) continue;

         std::string topic = rec["topic"].s();
         int64_t ts = rec["timestamp_ms"].i();
         int64_t offset = ts - baseTime;

         // Honour original inter-message timing (capped at 5 s)
         int64_t delta = offset - prevOffset;
         if (delta > 0)
         {
            auto sleepMs = std::min(delta, static_cast<int64_t>(5000));
            std::unique_lock lock(_stopMutex);
            _stopCv.wait_for(lock, std::chrono::milliseconds(sleepMs),
               [this]() { return _stopFlag.load(); });
         }
         prevOffset = offset;

         if (_stopFlag.load()) break;

         // Republish through the routed transport
         std::string jsonData = crow::json::wvalue(rec["data"]).dump();
         _publish(topic, jsonData);

         if (onProgress) onProgress(i + 1, total);
      }

      bool stopped = _stopFlag.load();
      _playing.store(false);

      GPINFO("PlaybackEngine: playback {}", stopped ? "stopped" : "finished");
      if (onComplete) onComplete(stopped);
   });
}

void PlaybackEngine::stop()
{
   _stopFlag.store(true);
   _stopCv.notify_all();
   if (_thread.joinable())
      _thread.join();
   _playing.store(false);
}

bool PlaybackEngine::isPlaying() const
{
   return _playing.load();
}

} // namespace Omniscope
