#ifndef OBSERVABILITY_METRICSCONFIG_H_
#define OBSERVABILITY_METRICSCONFIG_H_

// System headers
#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Observability
{

/**
 * @brief Supported metrics exporter targets.
 */
enum class MetricsExporterKind
{
   Grpc,
   Http,
   Console,
};

/**
 * @brief YAML-backed runtime configuration for OTel metrics.
 */
class MetricsConfig
{
public:
   /**
    * @brief Build defaults for a given service name.
    */
   static MetricsConfig defaultsForService(std::string serviceName);

   /**
    * @brief Parse config from a YAML file path.
    */
   static MetricsConfig fromYamlFile(const std::string &filePath);

   /**
    * @brief Parse config from a YAML text payload.
    */
   static MetricsConfig fromYamlString(std::string_view yamlText);

   [[nodiscard]] const std::string &serviceName() const;
   [[nodiscard]] MetricsExporterKind exporterKind() const;
   [[nodiscard]] const std::chrono::milliseconds &aggregationPeriod() const;
   [[nodiscard]] const std::chrono::milliseconds &exportTimeout() const;
   [[nodiscard]] const std::string &endpoint() const;
   [[nodiscard]] bool insecure() const;
   [[nodiscard]] const std::unordered_map<std::string, std::string> &headers() const;
   [[nodiscard]] const std::unordered_map<std::string, std::string> &resourceAttributes() const;
   [[nodiscard]] const std::string &meterName() const;
   [[nodiscard]] const std::string &meterVersion() const;
   [[nodiscard]] bool interfaceHistogramBytesPerCallEnabled() const;
   [[nodiscard]] bool interfaceHistogramBytesPerSecondEnabled() const;
   [[nodiscard]] bool processMetricsEnabled() const;
   [[nodiscard]] const std::chrono::milliseconds &processSamplingPeriod() const;
   [[nodiscard]] bool processGaugeCpuUtilizationEnabled() const;
   [[nodiscard]] bool processGaugeMemoryRssEnabled() const;
   [[nodiscard]] bool processGaugeMemoryVmsEnabled() const;
   [[nodiscard]] bool processGaugeThreadCountEnabled() const;
   [[nodiscard]] bool gpuMetricsEnabled() const;
   [[nodiscard]] const std::chrono::milliseconds &gpuSamplingPeriod() const;
   [[nodiscard]] bool gpuGaugeUtilizationEnabled() const;
   [[nodiscard]] bool gpuGaugeMemoryUsedEnabled() const;
   [[nodiscard]] bool gpuGaugeMemoryTotalEnabled() const;

   void setServiceName(const std::string &value);
   void setExporterKind(MetricsExporterKind value);
   void setAggregationPeriod(std::chrono::milliseconds value);
   void setExportTimeout(std::chrono::milliseconds value);
   void setEndpoint(const std::string &value);
   void setInsecure(bool value);
   void setMeterName(const std::string &value);
   void setMeterVersion(const std::string &value);
   void setHeaders(const std::unordered_map<std::string, std::string> &value);
   void setResourceAttributes(const std::unordered_map<std::string, std::string> &value);
   void setInterfaceHistogramBytesPerCallEnabled(bool value);
   void setInterfaceHistogramBytesPerSecondEnabled(bool value);
   void setProcessMetricsEnabled(bool value);
   void setProcessSamplingPeriod(std::chrono::milliseconds value);
   void setProcessGaugeCpuUtilizationEnabled(bool value);
   void setProcessGaugeMemoryRssEnabled(bool value);
   void setProcessGaugeMemoryVmsEnabled(bool value);
   void setProcessGaugeThreadCountEnabled(bool value);
   void setGpuMetricsEnabled(bool value);
   void setGpuSamplingPeriod(std::chrono::milliseconds value);
   void setGpuGaugeUtilizationEnabled(bool value);
   void setGpuGaugeMemoryUsedEnabled(bool value);
   void setGpuGaugeMemoryTotalEnabled(bool value);

private:
   std::string _serviceName{"ContainerizedCPP"};
   MetricsExporterKind _exporterKind{MetricsExporterKind::Grpc};
   std::chrono::milliseconds _aggregationPeriod{5000};
   std::chrono::milliseconds _exportTimeout{5000};
   std::string _endpoint{"http://localhost:4317"};
   bool _insecure{true};
   std::unordered_map<std::string, std::string> _headers;
   std::unordered_map<std::string, std::string> _resourceAttributes;
   std::string _meterName{"Observability"};
   std::string _meterVersion;
   bool _interfaceHistogramBytesPerCallEnabled{true};
   bool _interfaceHistogramBytesPerSecondEnabled{true};
   bool _processMetricsEnabled{false};
   std::chrono::milliseconds _processSamplingPeriod{1000};
   bool _processGaugeCpuUtilizationEnabled{true};
   bool _processGaugeMemoryRssEnabled{true};
   bool _processGaugeMemoryVmsEnabled{true};
   bool _processGaugeThreadCountEnabled{true};
   bool _gpuMetricsEnabled{false};
   std::chrono::milliseconds _gpuSamplingPeriod{1000};
   bool _gpuGaugeUtilizationEnabled{true};
   bool _gpuGaugeMemoryUsedEnabled{true};
   bool _gpuGaugeMemoryTotalEnabled{true};
};

} // namespace Observability

#endif // OBSERVABILITY_METRICSCONFIG_H_