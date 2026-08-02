#include "DashboardApp.h"

#include "CommonUtils/GeneralLogger.h"

#include <crow.h>

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================================
//  Embedded web UI
// ============================================================================

const std::string DASHBOARD_HTML =
#include "web/monitor.html.inc"
;

const std::string DASHBOARD_CSS =
#include "web/style.css.inc"
;

const std::string DASHBOARD_JS =
#include "web/app.js.inc"
;

namespace ObservabilityDashboard
{

namespace
{

// ----------------------------------------------------------------------------
//  In-memory metrics store
// ----------------------------------------------------------------------------

struct MetricPoint
{
   double value;
   std::map<std::string, std::string> attributes;
};

struct ServiceSnapshot
{
   std::chrono::system_clock::time_point updatedAt;
   std::unordered_map<std::string, std::vector<MetricPoint>> metrics; // metric name -> data points
};

// ----------------------------------------------------------------------------
//  OTLP/HTTP JSON parsing (see
//  https://github.com/open-telemetry/opentelemetry-proto/blob/main/docs/specification.md)
//
//  Deliberately configured on the sender side (MetricsExporterFactory.cpp)
//  to use JSON rather than the default binary protobuf, so this receiver
//  can stay a plain JSON parser -- no protobuf tooling needed.
// ----------------------------------------------------------------------------

std::string findServiceName(const crow::json::rvalue &resource)
{
   if (!resource.has("attributes"))
   {
      return "unknown";
   }
   for (const auto &attr : resource["attributes"])
   {
      if (attr.has("key") && std::string(attr["key"].s()) == "service.name" && attr.has("value") &&
          attr["value"].has("stringValue"))
      {
         return attr["value"]["stringValue"].s();
      }
   }
   return "unknown";
}

std::map<std::string, std::string> extractAttributes(const crow::json::rvalue &dataPoint)
{
   std::map<std::string, std::string> attrs;
   if (!dataPoint.has("attributes"))
   {
      return attrs;
   }
   for (const auto &attr : dataPoint["attributes"])
   {
      if (!attr.has("key") || !attr.has("value"))
      {
         continue;
      }
      std::string key = attr["key"].s();
      const auto &value = attr["value"];
      if (value.has("stringValue"))
      {
         attrs[key] = value["stringValue"].s();
      }
      else if (value.has("intValue"))
      {
         attrs[key] = std::to_string(value["intValue"].i());
      }
      else if (value.has("doubleValue"))
      {
         attrs[key] = std::to_string(value["doubleValue"].d());
      }
      else if (value.has("boolValue"))
      {
         attrs[key] = value["boolValue"].b() ? "true" : "false";
      }
   }
   return attrs;
}

// sum/gauge data points carry the value directly. OTLP JSON encodes 64-bit
// integers as JSON strings (asInt) to avoid precision loss; crow's
// rvalue::i() handles both Number and String tokens via lexical_cast, so
// this works regardless of which one arrives.
double extractNumericValue(const crow::json::rvalue &dataPoint)
{
   if (dataPoint.has("asInt"))
   {
      return static_cast<double>(dataPoint["asInt"].i());
   }
   if (dataPoint.has("asDouble"))
   {
      return dataPoint["asDouble"].d();
   }
   return 0.0;
}

// Histogram data points don't carry a single "value" -- reduce to the mean
// (sum/count) since that's what a single-number dashboard tile can show.
double extractHistogramAverage(const crow::json::rvalue &dataPoint)
{
   if (!dataPoint.has("count"))
   {
      return 0.0;
   }
   double count = static_cast<double>(dataPoint["count"].i());
   if (count <= 0.0)
   {
      return 0.0;
   }
   double sum = dataPoint.has("sum") ? dataPoint["sum"].d() : 0.0;
   return sum / count;
}

void processMetric(const crow::json::rvalue &metric, ServiceSnapshot &snapshot)
{
   if (!metric.has("name"))
   {
      return;
   }
   std::string name = metric["name"].s();

   std::vector<MetricPoint> points;

   if (metric.has("sum") && metric["sum"].has("dataPoints"))
   {
      for (const auto &dp : metric["sum"]["dataPoints"])
      {
         points.push_back(MetricPoint{extractNumericValue(dp), extractAttributes(dp)});
      }
   }
   else if (metric.has("gauge") && metric["gauge"].has("dataPoints"))
   {
      for (const auto &dp : metric["gauge"]["dataPoints"])
      {
         points.push_back(MetricPoint{extractNumericValue(dp), extractAttributes(dp)});
      }
   }
   else if (metric.has("histogram") && metric["histogram"].has("dataPoints"))
   {
      for (const auto &dp : metric["histogram"]["dataPoints"])
      {
         points.push_back(MetricPoint{extractHistogramAverage(dp), extractAttributes(dp)});
      }
   }
   else
   {
      return; // unrecognized instrument shape -- ignore rather than guess
   }

   snapshot.metrics[name] = std::move(points);
}

} // namespace

// ============================================================================
//  DashboardApp::Impl
// ============================================================================

struct DashboardApp::Impl
{
   explicit Impl(uint16_t httpPortArg)
      : httpPort(httpPortArg)
   {
   }

   uint16_t httpPort;
   crow::SimpleApp crowApp;

   std::unordered_set<crow::websocket::connection *> wsConnections;
   std::mutex wsMutex;

   std::mutex storeMutex;
   std::unordered_map<std::string, ServiceSnapshot> snapshotsByService;

   // --- Serialization (call with storeMutex held) ----------------------------

   crow::json::wvalue pointToJson(const MetricPoint &point) const
   {
      crow::json::wvalue json;
      json["value"] = point.value;
      crow::json::wvalue attrs;
      for (const auto &kv : point.attributes)
      {
         attrs[kv.first] = kv.second;
      }
      json["attributes"] = std::move(attrs);
      return json;
   }

   crow::json::wvalue snapshotToJson(const ServiceSnapshot &snapshot) const
   {
      crow::json::wvalue json;
      auto updatedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           snapshot.updatedAt.time_since_epoch())
                           .count();
      json["updatedAtMs"] = updatedMs;

      crow::json::wvalue metricsJson;
      for (const auto &entry : snapshot.metrics)
      {
         std::vector<crow::json::wvalue> points;
         for (const auto &point : entry.second)
         {
            points.push_back(pointToJson(point));
         }
         metricsJson[entry.first] = std::move(points);
      }
      json["metrics"] = std::move(metricsJson);
      return json;
   }

   std::string storeToJson() const
   {
      crow::json::wvalue json;
      json["type"] = "snapshot";
      crow::json::wvalue services;
      for (const auto &entry : snapshotsByService)
      {
         services[entry.first] = snapshotToJson(entry.second);
      }
      json["services"] = std::move(services);
      return json.dump();
   }

   // --- OTLP export handling --------------------------------------------------

   void handleExport(const crow::json::rvalue &body)
   {
      if (!body.has("resourceMetrics"))
      {
         return;
      }

      std::lock_guard<std::mutex> lock(storeMutex);

      for (const auto &resourceMetric : body["resourceMetrics"])
      {
         std::string serviceName =
            resourceMetric.has("resource") ? findServiceName(resourceMetric["resource"]) : "unknown";

         auto &snapshot = snapshotsByService[serviceName];
         snapshot.updatedAt = std::chrono::system_clock::now();

         if (!resourceMetric.has("scopeMetrics"))
         {
            continue;
         }
         for (const auto &scopeMetric : resourceMetric["scopeMetrics"])
         {
            if (!scopeMetric.has("metrics"))
            {
               continue;
            }
            for (const auto &metric : scopeMetric["metrics"])
            {
               processMetric(metric, snapshot);
            }
         }
      }
   }

   // --- WebSocket broadcast ---------------------------------------------------

   void broadcastSnapshot()
   {
      std::string payload;
      {
         std::lock_guard<std::mutex> lock(storeMutex);
         payload = storeToJson();
      }
      std::lock_guard<std::mutex> lock(wsMutex);
      for (auto *conn : wsConnections)
      {
         conn->send_text(payload);
      }
   }

   // --- Crow route setup --------------------------------------------------

   void setupRoutes()
   {
      CROW_ROUTE(crowApp, "/")
      ([]()
       {
          crow::response resp(DASHBOARD_HTML);
          resp.set_header("Content-Type", "text/html; charset=utf-8");
          return resp;
       });

      CROW_ROUTE(crowApp, "/style.css")
      ([]()
       {
          crow::response resp(DASHBOARD_CSS);
          resp.set_header("Content-Type", "text/css; charset=utf-8");
          return resp;
       });

      CROW_ROUTE(crowApp, "/app.js")
      ([]()
       {
          crow::response resp(DASHBOARD_JS);
          resp.set_header("Content-Type", "text/javascript; charset=utf-8");
          return resp;
       });

      // OTLP/HTTP metrics receiver -- point Observability::MetricsOptions
      // (protocol Http) at http://localhost:<port>/v1/metrics.
      CROW_ROUTE(crowApp, "/v1/metrics")
         .methods(crow::HTTPMethod::POST)
      ([this](const crow::request &req)
       {
          auto body = crow::json::load(req.body);
          if (!body)
          {
             return crow::response(400, "invalid JSON body");
          }
          handleExport(body);
          broadcastSnapshot();
          return crow::response(200);
       });

      // Debug/inspection snapshot without opening the browser.
      CROW_ROUTE(crowApp, "/api/metrics")
      ([this]()
       {
          std::string payload;
          {
             std::lock_guard<std::mutex> lock(storeMutex);
             payload = storeToJson();
          }
          crow::response resp(payload);
          resp.set_header("Content-Type", "application/json; charset=utf-8");
          return resp;
       });

      CROW_WEBSOCKET_ROUTE(crowApp, "/ws")
         .onopen(
            [this](crow::websocket::connection &conn)
            {
               GPINFO("ObservabilityDashboard: WebSocket client connected");
               {
                  std::lock_guard<std::mutex> lock(wsMutex);
                  wsConnections.insert(&conn);
               }
               // Send the current snapshot immediately so a newly-connected
               // client isn't blank until the next export arrives.
               std::string payload;
               {
                  std::lock_guard<std::mutex> lock(storeMutex);
                  payload = storeToJson();
               }
               conn.send_text(payload);
            })
         .onclose(
            [this](crow::websocket::connection &conn, const std::string & /*reason*/)
            {
               GPINFO("ObservabilityDashboard: WebSocket client disconnected");
               std::lock_guard<std::mutex> lock(wsMutex);
               wsConnections.erase(&conn);
            })
         .onmessage([](crow::websocket::connection & /*conn*/, const std::string & /*msg*/,
                       bool /*isBinary*/) { /* read-only dashboard -- no client commands */ });
   }
};

// ============================================================================
//  DashboardApp public API
// ============================================================================

DashboardApp::DashboardApp(uint16_t httpPort)
   : _impl(std::make_unique<Impl>(httpPort))
{
}

DashboardApp::~DashboardApp() = default;

void DashboardApp::run()
{
   _impl->setupRoutes();

   GPINFO("ObservabilityDashboard starting on http://localhost:{}", _impl->httpPort);
   GPINFO("  OTLP/HTTP metrics receiver: http://localhost:{}/v1/metrics", _impl->httpPort);

   // Crow handles SIGINT/SIGTERM by default and returns from run().
   _impl->crowApp.port(_impl->httpPort).multithreaded().run();

   GPINFO("ObservabilityDashboard shutting down...");
}

} // namespace ObservabilityDashboard
