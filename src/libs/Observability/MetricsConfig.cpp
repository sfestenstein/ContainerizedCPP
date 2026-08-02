#include "Observability/MetricsConfig.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace Observability
{

namespace
{

MetricsExporterKind parseExporterKind(const std::string &value)
{
   std::string normalized = value;
   std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                  [](unsigned char c)
                  {
                     return static_cast<char>(std::tolower(c));
                  });

   if (normalized == "grpc")
   {
      return MetricsExporterKind::Grpc;
   }
   if (normalized == "http")
   {
      return MetricsExporterKind::Http;
   }
   if (normalized == "console")
   {
      return MetricsExporterKind::Console;
   }

   throw std::runtime_error("Unsupported metrics exporter protocol: " + value);
}

void readStringMap(const YAML::Node &node, std::unordered_map<std::string, std::string> &out)
{
   if (!node || !node.IsMap())
   {
      return;
   }

   for (const auto &entry : node)
   {
      out[entry.first.as<std::string>()] = entry.second.as<std::string>();
   }
}

MetricsConfig fromYamlNode(const YAML::Node &root)
{
   MetricsConfig config;

   const YAML::Node source = root["metrics"] ? root["metrics"] : root;

   if (source["service_name"])
   {
      config.setServiceName(source["service_name"].as<std::string>());
   }

   if (source["aggregation_period_ms"])
   {
      config.setAggregationPeriod(
         std::chrono::milliseconds(source["aggregation_period_ms"].as<int64_t>()));
   }
   if (source["timeout_ms"])
   {
      config.setExportTimeout(std::chrono::milliseconds(source["timeout_ms"].as<int64_t>()));
   }
   if (source["meter_name"])
   {
      config.setMeterName(source["meter_name"].as<std::string>());
   }
   if (source["meter_version"])
   {
      config.setMeterVersion(source["meter_version"].as<std::string>());
   }

   std::unordered_map<std::string, std::string> resourceAttributes;
   readStringMap(source["resource_attributes"], resourceAttributes);
   config.setResourceAttributes(resourceAttributes);

   const YAML::Node exporter = source["exporter"] ? source["exporter"] : source;
   if (exporter["protocol"])
   {
      config.setExporterKind(parseExporterKind(exporter["protocol"].as<std::string>()));
   }
   if (exporter["endpoint"])
   {
      config.setEndpoint(exporter["endpoint"].as<std::string>());
   }
   if (exporter["insecure"])
   {
      config.setInsecure(exporter["insecure"].as<bool>());
   }

   std::unordered_map<std::string, std::string> headers;
   readStringMap(exporter["headers"], headers);
   config.setHeaders(headers);

   const YAML::Node interfaceHistograms = source["interface_histograms"];
   if (interfaceHistograms)
   {
      if (interfaceHistograms["bytes_per_call"])
      {
         config.setInterfaceHistogramBytesPerCallEnabled(interfaceHistograms["bytes_per_call"].as<bool>());
      }
      if (interfaceHistograms["bytes_per_second"])
      {
         config.setInterfaceHistogramBytesPerSecondEnabled(interfaceHistograms["bytes_per_second"].as<bool>());
      }
   }

   const YAML::Node processMetrics = source["process_metrics"];
   if (processMetrics)
   {
      if (processMetrics["enabled"])
      {
         config.setProcessMetricsEnabled(processMetrics["enabled"].as<bool>());
      }
      if (processMetrics["sampling_period_ms"])
      {
         const auto samplingPeriod = processMetrics["sampling_period_ms"].as<int64_t>();
         if (samplingPeriod <= 0)
         {
            throw std::runtime_error("process_metrics.sampling_period_ms must be > 0");
         }
         config.setProcessSamplingPeriod(std::chrono::milliseconds(samplingPeriod));
      }

      const YAML::Node gauges = processMetrics["gauges"];
      if (gauges)
      {
         if (gauges["cpu_utilization"])
         {
            config.setProcessGaugeCpuUtilizationEnabled(gauges["cpu_utilization"].as<bool>());
         }
         if (gauges["memory_rss"])
         {
            config.setProcessGaugeMemoryRssEnabled(gauges["memory_rss"].as<bool>());
         }
         if (gauges["memory_vms"])
         {
            config.setProcessGaugeMemoryVmsEnabled(gauges["memory_vms"].as<bool>());
         }
         if (gauges["thread_count"])
         {
            config.setProcessGaugeThreadCountEnabled(gauges["thread_count"].as<bool>());
         }
      }
   }

   const YAML::Node gpuMetrics = source["gpu_metrics"];
   if (gpuMetrics)
   {
      if (gpuMetrics["enabled"])
      {
         config.setGpuMetricsEnabled(gpuMetrics["enabled"].as<bool>());
      }
      if (gpuMetrics["sampling_period_ms"])
      {
         const auto samplingPeriod = gpuMetrics["sampling_period_ms"].as<int64_t>();
         if (samplingPeriod <= 0)
         {
            throw std::runtime_error("gpu_metrics.sampling_period_ms must be > 0");
         }
         config.setGpuSamplingPeriod(std::chrono::milliseconds(samplingPeriod));
      }

      const YAML::Node gpuGauges = gpuMetrics["gauges"];
      if (gpuGauges)
      {
         if (gpuGauges["utilization"])
         {
            config.setGpuGaugeUtilizationEnabled(gpuGauges["utilization"].as<bool>());
         }
         if (gpuGauges["memory_used"])
         {
            config.setGpuGaugeMemoryUsedEnabled(gpuGauges["memory_used"].as<bool>());
         }
         if (gpuGauges["memory_total"])
         {
            config.setGpuGaugeMemoryTotalEnabled(gpuGauges["memory_total"].as<bool>());
         }
      }
   }

   return config;
}

} // namespace

MetricsConfig MetricsConfig::defaultsForService(std::string serviceName)
{
   MetricsConfig config;
   config._serviceName = std::move(serviceName);
   return config;
}

MetricsConfig MetricsConfig::fromYamlFile(const std::string &filePath)
{
   try
   {
      return fromYamlNode(YAML::LoadFile(filePath));
   }
   catch (const std::exception &ex)
   {
      throw std::runtime_error("Failed to load metrics config file '" + filePath + "': " + ex.what());
   }
}

MetricsConfig MetricsConfig::fromYamlString(std::string_view yamlText)
{
   try
   {
      return fromYamlNode(YAML::Load(std::string(yamlText)));
   }
   catch (const std::exception &ex)
   {
      throw std::runtime_error("Failed to parse metrics YAML config: " + std::string(ex.what()));
   }
}

const std::string &MetricsConfig::serviceName() const
{
   return _serviceName;
}

MetricsExporterKind MetricsConfig::exporterKind() const
{
   return _exporterKind;
}

const std::chrono::milliseconds &MetricsConfig::aggregationPeriod() const
{
   return _aggregationPeriod;
}

const std::chrono::milliseconds &MetricsConfig::exportTimeout() const
{
   return _exportTimeout;
}

const std::string &MetricsConfig::endpoint() const
{
   return _endpoint;
}

bool MetricsConfig::insecure() const
{
   return _insecure;
}

const std::unordered_map<std::string, std::string> &MetricsConfig::headers() const
{
   return _headers;
}

const std::unordered_map<std::string, std::string> &MetricsConfig::resourceAttributes() const
{
   return _resourceAttributes;
}

const std::string &MetricsConfig::meterName() const
{
   return _meterName;
}

const std::string &MetricsConfig::meterVersion() const
{
   return _meterVersion;
}

bool MetricsConfig::interfaceHistogramBytesPerCallEnabled() const
{
   return _interfaceHistogramBytesPerCallEnabled;
}

bool MetricsConfig::interfaceHistogramBytesPerSecondEnabled() const
{
   return _interfaceHistogramBytesPerSecondEnabled;
}

bool MetricsConfig::processMetricsEnabled() const
{
   return _processMetricsEnabled;
}

const std::chrono::milliseconds &MetricsConfig::processSamplingPeriod() const
{
   return _processSamplingPeriod;
}

bool MetricsConfig::processGaugeCpuUtilizationEnabled() const
{
   return _processGaugeCpuUtilizationEnabled;
}

bool MetricsConfig::processGaugeMemoryRssEnabled() const
{
   return _processGaugeMemoryRssEnabled;
}

bool MetricsConfig::processGaugeMemoryVmsEnabled() const
{
   return _processGaugeMemoryVmsEnabled;
}

bool MetricsConfig::processGaugeThreadCountEnabled() const
{
   return _processGaugeThreadCountEnabled;
}

void MetricsConfig::setServiceName(const std::string &value)
{
   _serviceName = value;
}

void MetricsConfig::setExporterKind(MetricsExporterKind value)
{
   _exporterKind = value;
}

void MetricsConfig::setAggregationPeriod(std::chrono::milliseconds value)
{
   _aggregationPeriod = value;
}

void MetricsConfig::setExportTimeout(std::chrono::milliseconds value)
{
   _exportTimeout = value;
}

void MetricsConfig::setEndpoint(const std::string &value)
{
   _endpoint = value;
}

void MetricsConfig::setInsecure(bool value)
{
   _insecure = value;
}

void MetricsConfig::setMeterName(const std::string &value)
{
   _meterName = value;
}

void MetricsConfig::setMeterVersion(const std::string &value)
{
   _meterVersion = value;
}

void MetricsConfig::setHeaders(const std::unordered_map<std::string, std::string> &value)
{
   _headers = value;
}

void MetricsConfig::setResourceAttributes(const std::unordered_map<std::string, std::string> &value)
{
   _resourceAttributes = value;
}

void MetricsConfig::setInterfaceHistogramBytesPerCallEnabled(bool value)
{
   _interfaceHistogramBytesPerCallEnabled = value;
}

void MetricsConfig::setInterfaceHistogramBytesPerSecondEnabled(bool value)
{
   _interfaceHistogramBytesPerSecondEnabled = value;
}

void MetricsConfig::setProcessMetricsEnabled(bool value)
{
   _processMetricsEnabled = value;
}

void MetricsConfig::setProcessSamplingPeriod(std::chrono::milliseconds value)
{
   _processSamplingPeriod = value;
}

void MetricsConfig::setProcessGaugeCpuUtilizationEnabled(bool value)
{
   _processGaugeCpuUtilizationEnabled = value;
}

void MetricsConfig::setProcessGaugeMemoryRssEnabled(bool value)
{
   _processGaugeMemoryRssEnabled = value;
}

void MetricsConfig::setProcessGaugeMemoryVmsEnabled(bool value)
{
   _processGaugeMemoryVmsEnabled = value;
}

void MetricsConfig::setProcessGaugeThreadCountEnabled(bool value)
{
   _processGaugeThreadCountEnabled = value;
}

bool MetricsConfig::gpuMetricsEnabled() const
{
   return _gpuMetricsEnabled;
}

const std::chrono::milliseconds &MetricsConfig::gpuSamplingPeriod() const
{
   return _gpuSamplingPeriod;
}

bool MetricsConfig::gpuGaugeUtilizationEnabled() const
{
   return _gpuGaugeUtilizationEnabled;
}

bool MetricsConfig::gpuGaugeMemoryUsedEnabled() const
{
   return _gpuGaugeMemoryUsedEnabled;
}

bool MetricsConfig::gpuGaugeMemoryTotalEnabled() const
{
   return _gpuGaugeMemoryTotalEnabled;
}

void MetricsConfig::setGpuMetricsEnabled(bool value)
{
   _gpuMetricsEnabled = value;
}

void MetricsConfig::setGpuSamplingPeriod(std::chrono::milliseconds value)
{
   _gpuSamplingPeriod = value;
}

void MetricsConfig::setGpuGaugeUtilizationEnabled(bool value)
{
   _gpuGaugeUtilizationEnabled = value;
}

void MetricsConfig::setGpuGaugeMemoryUsedEnabled(bool value)
{
   _gpuGaugeMemoryUsedEnabled = value;
}

void MetricsConfig::setGpuGaugeMemoryTotalEnabled(bool value)
{
   _gpuGaugeMemoryTotalEnabled = value;
}

} // namespace Observability