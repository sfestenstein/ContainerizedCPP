#include "Observability/MetricsConfig.h"

#include <yaml-cpp/yaml.h>

#include <stdexcept>

namespace Observability
{

namespace
{

ExporterProtocol parseProtocol(const std::string &value)
{
   if (value == "grpc")
   {
      return ExporterProtocol::Grpc;
   }
   if (value == "http")
   {
      return ExporterProtocol::Http;
   }
   if (value == "console")
   {
      return ExporterProtocol::Console;
   }
   throw std::runtime_error("Observability config: unknown protocol '" + value +
                             "' (expected 'grpc', 'http', or 'console')");
}

} // namespace

MetricsOptions loadMetricsOptions(const std::string &yamlPath)
{
   YAML::Node root;
   try
   {
      root = YAML::LoadFile(yamlPath);
   }
   catch (const YAML::Exception &e)
   {
      throw std::runtime_error("Observability config '" + yamlPath + "': " + e.what());
   }

   YAML::Node node = root["observability"];
   if (!node || !node.IsMap())
   {
      throw std::runtime_error("Observability config '" + yamlPath +
                                "': missing top-level 'observability' map");
   }

   MetricsOptions options;

   try
   {
      options.aggregationPeriod =
         std::chrono::milliseconds(node["aggregationPeriodMs"].as<int64_t>(options.aggregationPeriod.count()));
      options.exportTimeout =
         std::chrono::milliseconds(node["exportTimeoutMs"].as<int64_t>(options.exportTimeout.count()));
      options.protocol = parseProtocol(node["protocol"].as<std::string>("grpc"));
      options.endpoint = node["endpoint"].as<std::string>("");
   }
   catch (const YAML::Exception &e)
   {
      throw std::runtime_error("Observability config '" + yamlPath + "': " + e.what());
   }

   if (options.exportTimeout <= std::chrono::milliseconds::zero() ||
       options.exportTimeout >= options.aggregationPeriod)
   {
      throw std::runtime_error("Observability config '" + yamlPath +
                                "': exportTimeoutMs must be greater than 0 and less than aggregationPeriodMs");
   }

   return options;
}

} // namespace Observability
