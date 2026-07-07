/**
 * @file main.cpp
 * @brief Entry point for the Omniscope application.
 *
 * Usage:
 *   Omniscope [domain_id] [http_port]
 *   Open http://localhost:<port>  (default 8080)
 */

#include "OmniscopeApp.h"
#include "TransportDds.h"
#include "TransportZyre.h"
#include "CommonUtils/GeneralLogger.h"

#include <cstdint>
#include <memory>
#include <string>

int main(int argc, char *argv[])
{
   CommonUtils::GeneralLogger logger;
   logger.init("Omniscope");

   uint32_t domainId = 0;
   uint16_t httpPort = 8080;
   std::string zyreNamespace = "TestZyre";

   if (argc > 1) domainId = static_cast<uint32_t>(std::stoul(argv[1]));
   if (argc > 2) httpPort = static_cast<uint16_t>(std::stoul(argv[2]));
   if (argc > 3) zyreNamespace = argv[3];

   Omniscope::OmniscopeApp app(httpPort);
   app.addTransport(std::make_unique<Omniscope::TransportDds>(domainId));
   app.addTransport(std::make_unique<Omniscope::TransportZyre>(zyreNamespace));
   app.run();

   return 0;
}
