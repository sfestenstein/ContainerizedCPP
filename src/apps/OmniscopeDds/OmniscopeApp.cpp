#include "OmniscopeApp.h"
#include "PlaybackEngine.h"

#include "CommonUtils/GeneralLogger.h"

#include <crow.h>

#include <cassert>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_set>

// ============================================================================
//  Embedded web UI
// ============================================================================

const std::string MONITOR_HTML =
#include "web/monitor.html.inc"
;

const std::string MONITOR_CSS =
#include "web/style.css.inc"
;

const std::string MONITOR_JS =
#include "web/app.js.inc"
;

namespace Omniscope
{

// ============================================================================
//  OmniscopeApp::Impl
// ============================================================================

struct OmniscopeApp::Impl
{
   explicit Impl(uint16_t httpPortArg)
      : httpPort(httpPortArg)
   {
   }

   // --- Transports ----------------------------------------------------------

   std::vector<std::unique_ptr<ITransport>> transports;

   ITransport *findTransport(const std::string &topic)
   {
      for (auto &t : transports)
      {
         for (const auto &tn : t->topicNames())
         {
            if (tn == topic) return t.get();
         }
      }
      return nullptr;
   }

   // --- Web server ----------------------------------------------------------

   uint16_t httpPort;
   crow::SimpleApp crowApp;

   std::unordered_set<crow::websocket::connection *> wsConnections;
   std::mutex wsMutex;

   void broadcastWs(const std::string &payload)
   {
      std::lock_guard lock(wsMutex);
      for (auto *conn : wsConnections)
         conn->send_text(payload);
   }

   // --- Recording -----------------------------------------------------------

   bool recording = false;
   std::ofstream recFile;
   std::string recFilename;
   std::mutex recMutex;

   // --- Playback ------------------------------------------------------------

   std::optional<PlaybackEngine> playback;

   // --- Message handling ----------------------------------------------------

   void onTransportMessage(const std::string &topic, const std::string &jsonData)
   {
      auto now = std::chrono::system_clock::now();
      auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
         now.time_since_epoch()).count();

      // ISO-8601 timestamp
      auto timeT = std::chrono::system_clock::to_time_t(now);
      auto ms = epochMs % 1000;
      std::tm tm{};
      (void)gmtime_r(&timeT, &tm);
      auto timestamp = std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
         tm.tm_hour, tm.tm_min, tm.tm_sec, ms);

      crow::json::wvalue envelope;
      envelope["type"]      = "message";
      envelope["topic"]     = topic;
      envelope["timestamp"] = timestamp;
      envelope["data"]      = crow::json::load(jsonData);

      std::string payload = envelope.dump();

      // Record if active
      {
         std::lock_guard lock(recMutex);
         if (recording && recFile.is_open())
         {
            crow::json::wvalue rec;
            rec["topic"]        = topic;
            rec["timestamp"]    = timestamp;
            rec["timestamp_ms"] = epochMs;
            rec["data"]         = crow::json::load(jsonData);
            recFile << rec.dump() << '\n';
         }
      }

      broadcastWs(payload);
   }

   // --- Topic list ----------------------------------------------------------

   void broadcastTopicList()
   {
      std::lock_guard lock(wsMutex);
      for (auto *conn : wsConnections)
         sendTopicList(*conn);
   }

   void sendTopicList(crow::websocket::connection &conn)
   {
      crow::json::wvalue msg;
      msg["type"] = "topics";

      std::vector<crow::json::wvalue> topicArr;
      for (auto &t : transports)
      {
         for (const auto &tn : t->topicNames())
         {
            crow::json::wvalue entry;
            entry["name"]       = tn;
            entry["subscribed"] = t->isSubscribed(tn);
            entry["transport"]  = t->name();
            topicArr.push_back(std::move(entry));
         }
      }

      msg["topics"] = std::move(topicArr);
      conn.send_text(msg.dump());
   }

   // --- Recording controls --------------------------------------------------

   void startRecording()
   {
      std::lock_guard lock(recMutex);
      if (recording) return;

      auto now = std::chrono::system_clock::now();
      auto epochSec = std::chrono::duration_cast<std::chrono::seconds>(
         now.time_since_epoch()).count();
      recFilename = "omni_" + std::to_string(epochSec) + ".dat";

      recFile.open(recFilename, std::ios::out | std::ios::trunc);
      recording = true;
      GPINFO("Omniscope: recording started -> {}", recFilename);
   }

   std::string stopRecording()
   {
      std::lock_guard lock(recMutex);
      if (!recording) return {};

      recording = false;
      recFile.close();
      GPINFO("Omniscope: recording stopped -> {}", recFilename);
      return recFilename;
   }

   // --- WebSocket command dispatch ------------------------------------------

   void handleWsMessage(crow::websocket::connection &conn, const std::string &msg)
   {
      auto j = crow::json::load(msg);
      if (!j) return;

      std::string type = j["type"].s();

      if (type == "subscribe")
      {
         std::string topic = j["topic"].s();
         auto *transport = findTransport(topic);
         if (transport)
         {
            transport->subscribe(topic,
               [this](const std::string &t, const std::string &d) {
                  onTransportMessage(t, d);
               });
         }
         sendTopicList(conn);
      }
      else if (type == "unsubscribe")
      {
         std::string topic = j["topic"].s();
         auto *transport = findTransport(topic);
         if (transport) transport->unsubscribe(topic);
         sendTopicList(conn);
      }
      else if (type == "record_start")
      {
         startRecording();
         crow::json::wvalue resp;
         resp["type"] = "recording_started";
         conn.send_text(resp.dump());
      }
      else if (type == "record_stop")
      {
         auto filename = stopRecording();
         crow::json::wvalue resp;
         resp["type"] = "recording_stopped";
         resp["filename"] = filename;
         conn.send_text(resp.dump());
      }
      else if (type == "playback_start")
      {
         playback->start(
            // Progress callback
            [this](size_t current, size_t total) {
               crow::json::wvalue n;
               n["type"]    = "playback_progress";
               n["current"] = current;
               n["total"]   = total;
               broadcastWs(n.dump());
            },
            // Completion callback
            [this](bool wasStopped) {
               crow::json::wvalue n;
               n["type"] = wasStopped ? "playback_stopped" : "playback_finished";
               broadcastWs(n.dump());
            });

         // Notify start
         crow::json::wvalue n;
         n["type"]  = "playback_started";
         n["count"] = playback->messageCount();
         broadcastWs(n.dump());
      }
      else if (type == "playback_stop")
      {
         playback->stop();
      }
   }

   // --- Crow route setup ----------------------------------------------------

   void setupRoutes()
   {
      // Serve the web UI
      CROW_ROUTE(crowApp, "/")
      ([]() {
         crow::response resp(MONITOR_HTML);
         resp.set_header("Content-Type", "text/html; charset=utf-8");
         return resp;
      });

      CROW_ROUTE(crowApp, "/style.css")
      ([]() {
         crow::response resp(MONITOR_CSS);
         resp.set_header("Content-Type", "text/css; charset=utf-8");
         return resp;
      });

      CROW_ROUTE(crowApp, "/app.js")
      ([]() {
         crow::response resp(MONITOR_JS);
         resp.set_header("Content-Type", "text/javascript; charset=utf-8");
         return resp;
      });

      // Upload a recording (.dat) for playback
      CROW_ROUTE(crowApp, "/playback/load")
         .methods(crow::HTTPMethod::POST)
      ([this](const crow::request &req) {
         if (req.body.empty())
            return crow::response(400, "Empty body");

         auto count = playback->loadRecording(req.body);

         // Notify connected clients
         crow::json::wvalue notification;
         notification["type"]  = "recording_loaded";
         notification["count"] = count;
         broadcastWs(notification.dump());

         crow::json::wvalue resp;
         resp["lines"] = count;
         return crow::response(200, resp.dump());
      });

      // WebSocket endpoint
      CROW_WEBSOCKET_ROUTE(crowApp, "/ws")
         .onopen([this](crow::websocket::connection &conn) {
            GPINFO("Omniscope: WebSocket client connected");
            {
               std::lock_guard lock(wsMutex);
               wsConnections.insert(&conn);
            }
            sendTopicList(conn);
         })
         .onclose([this](crow::websocket::connection &conn,
                         const std::string & /*reason*/) {
            GPINFO("Omniscope: WebSocket client disconnected");
            std::lock_guard lock(wsMutex);
            wsConnections.erase(&conn);
         })
         .onmessage([this](crow::websocket::connection &conn,
                           const std::string &msg, bool /*isBinary*/) {
            handleWsMessage(conn, msg);
         });
   }
};

// ============================================================================
//  OmniscopeApp public API
// ============================================================================

OmniscopeApp::OmniscopeApp(uint16_t httpPort)
   : _impl(std::make_unique<Impl>(httpPort))
{
}

OmniscopeApp::~OmniscopeApp()
{
   if (_impl->playback)
      _impl->playback->stop();
}

void OmniscopeApp::addTransport(std::unique_ptr<ITransport> transport)
{
   transport->setTopicsChangedCallback([this]() { _impl->broadcastTopicList(); });
   _impl->transports.push_back(std::move(transport));
}

void OmniscopeApp::run()
{
   assert(!_impl->transports.empty() && "At least one transport must be added before run()");

   _impl->playback.emplace([this](const std::string &topic,
                                   const std::string &jsonData)
   {
      for (const auto &t : _impl->transports)
         t->publishFromJson(topic, jsonData);
   });
   _impl->setupRoutes();

   GPINFO("Omniscope starting on http://localhost:{}", _impl->httpPort);
   for (const auto &t : _impl->transports)
      GPINFO("  Transport: {} ({})", t->name(), t->topicNames().size());

   // Crow handles SIGINT/SIGTERM by default and returns from run()
   _impl->crowApp.port(_impl->httpPort).multithreaded().run();

   GPINFO("Omniscope shutting down...");
   _impl->playback->stop();
}

} // namespace Omniscope
