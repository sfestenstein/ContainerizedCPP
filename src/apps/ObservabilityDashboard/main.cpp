/**
 * @file main.cpp
 * @brief Entry point for ObservabilityDashboard.
 *
 * Terminates OTLP/HTTP JSON metric exports directly from Radar/Workstation
 * (or any app using the Observability module with protocol Http) and
 * renders them live in an embedded browser dashboard -- no otel-collector,
 * no Prometheus/Grafana.
 *
 * Usage:
 *   ObservabilityDashboard [http_port]
 *   Open http://localhost:<port>  (default 4318, the standard OTLP/HTTP port)
 *
 * Point an app's config at this dashboard by setting, e.g.
 * config/radar-observability.yaml's `endpoint` to
 * "http://localhost:<port>/v1/metrics" with `protocol: http`.
 */

#include "DashboardApp.h"
#include "CommonUtils/GeneralLogger.h"

#include <cstdint>
#include <string>

int main(int argc, char *argv[])
{
   CommonUtils::GeneralLogger logger;
   logger.init("ObservabilityDashboard");

   uint16_t httpPort = 4318;
   if (argc > 1)
   {
      httpPort = static_cast<uint16_t>(std::stoul(argv[1]));
   }

   ObservabilityDashboard::DashboardApp app(httpPort);
   app.run();

   return 0;
}
