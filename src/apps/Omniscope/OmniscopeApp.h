#ifndef OMNISCOPEAPP_H_
#define OMNISCOPEAPP_H_

#include "ITransport.h"

#include <cstdint>
#include <memory>

namespace Omniscope
{

/**
 * @brief Web-based IPC traffic monitor application.
 *
 * Orchestrates one or more ITransport instances, a Crow HTTP/WebSocket
 * server, message recording, and the PlaybackEngine.
 *
 * Workflow:
 *   1. Construct with an HTTP port.
 *   2. Add transports via addTransport().
 *   3. Call run() — blocks until interrupted (SIGINT / SIGTERM).
 *   4. Browser clients connect at http://localhost:<port>.
 */
class OmniscopeApp
{
public:
   explicit OmniscopeApp(uint16_t httpPortArg);
   ~OmniscopeApp();

   OmniscopeApp(const OmniscopeApp &) = delete;
   OmniscopeApp &operator=(const OmniscopeApp &) = delete;

   /// Register a transport.  Must be called before run().
   void addTransport(std::unique_ptr<ITransport> transport);

   /// Run the event loop.  Blocks until the global stop signal is raised.
   void run();

private:
   struct Impl;
   std::unique_ptr<Impl> _impl;
};

} // namespace Omniscope

#endif // OMNISCOPEAPP_H_
