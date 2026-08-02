#ifndef DASHBOARDAPP_H_
#define DASHBOARDAPP_H_

#include <cstdint>
#include <memory>

namespace ObservabilityDashboard
{

/**
 * @brief Live web dashboard that terminates OTLP/HTTP JSON metric exports
 *        directly (no otel-collector) and pushes them to connected browsers
 *        over a WebSocket.
 *
 * Structured the same way as Omniscope::OmniscopeApp: a Crow HTTP+WebSocket
 * server with the UI embedded into the binary at build time (see
 * EmbedAsset.cmake). Point a Radar/Workstation-style app's
 * Observability::MetricsOptions::endpoint at
 * "http://localhost:<port>/v1/metrics" with protocol Http to feed it.
 *
 * Workflow:
 *   1. Construct with an HTTP port.
 *   2. Call run() -- blocks until interrupted (SIGINT/SIGTERM).
 *   3. Browser clients connect at http://localhost:<port>.
 */
class DashboardApp
{
public:
   explicit DashboardApp(uint16_t httpPort);
   ~DashboardApp();

   DashboardApp(const DashboardApp &) = delete;
   DashboardApp &operator=(const DashboardApp &) = delete;

   /// Run the event loop. Blocks until interrupted.
   void run();

private:
   struct Impl;
   std::unique_ptr<Impl> _impl;
};

} // namespace ObservabilityDashboard

#endif // DASHBOARDAPP_H_
