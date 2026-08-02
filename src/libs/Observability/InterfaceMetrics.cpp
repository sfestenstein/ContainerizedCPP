#include "Observability/InterfaceMetrics.h"

#include <opentelemetry/exporters/ostream/metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/observer_result.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/nostd/variant.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>
#include <opentelemetry/sdk/resource/resource.h>

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <unistd.h>

namespace Observability
{

namespace
{

enum class Direction
{
   Sent,
   Received,
};

struct HistogramToggles
{
   std::atomic<bool> bytesPerCallEnabled{true};
   std::atomic<bool> bytesPerSecondEnabled{true};
};

struct RateState
{
   std::chrono::steady_clock::time_point lastSampleTime;
   bool initialized{false};
};

struct ProcessSnapshot
{
   double cpuUtilizationPercent{0.0};
   int64_t memoryRssBytes{0};
   int64_t memoryVmsBytes{0};
   int64_t threadCount{0};
};

std::string &configuredMeterName()
{
   static std::string meterName{"Observability"};
   return meterName;
}

std::string &configuredMeterVersion()
{
   static std::string meterVersion;
   return meterVersion;
}

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> getConfiguredMeter();

HistogramToggles &histogramToggles()
{
   static HistogramToggles toggles;
   return toggles;
}

std::mutex &rateStateMutex()
{
   static std::mutex mutex;
   return mutex;
}

std::unordered_map<std::string, RateState> &rateStates()
{
   static std::unordered_map<std::string, RateState> states;
   return states;
}

std::string makeRateKey(Direction direction,
                        const char *interfaceName,
                        const char *type,
                        const char *userAttribute)
{
   const std::string safeInterface = interfaceName != nullptr ? interfaceName : "";
   const std::string safeType = type != nullptr ? type : "";
   const std::string safeUser = userAttribute != nullptr ? userAttribute : "";
   const std::string prefix = direction == Direction::Sent ? "s|" : "r|";
   return prefix + safeInterface + "|" + safeType + "|" + safeUser;
}

void resetRateStates()
{
   std::lock_guard<std::mutex> lock(rateStateMutex());
   rateStates().clear();
}

bool readProcessStat(uint64_t &processJiffies, int64_t &threadCount)
{
   std::ifstream statFile("/proc/self/stat");
   if (!statFile.is_open())
   {
      return false;
   }

   std::string line;
   std::getline(statFile, line);
   if (line.empty())
   {
      return false;
   }

   const auto rightParen = line.rfind(')');
   if (rightParen == std::string::npos || rightParen + 2 >= line.size())
   {
      return false;
   }

   std::istringstream parser(line.substr(rightParen + 2));
   std::vector<std::string> fields;
   std::string field;
   while (parser >> field)
   {
      fields.push_back(field);
   }

   // Fields after the comm token start at position 3. We need utime(14),
   // stime(15), and num_threads(20), which map to indexes 11, 12, and 17.
   if (fields.size() <= 17)
   {
      return false;
   }

   try
   {
      const uint64_t utime = static_cast<uint64_t>(std::stoull(fields[11]));
      const uint64_t stime = static_cast<uint64_t>(std::stoull(fields[12]));
      processJiffies = utime + stime;
      threadCount = static_cast<int64_t>(std::stoll(fields[17]));
      return true;
   }
   catch (...)
   {
      return false;
   }
}

bool readProcessStatusMemory(int64_t &rssBytes, int64_t &vmsBytes, int64_t &threadCount)
{
   std::ifstream statusFile("/proc/self/status");
   if (!statusFile.is_open())
   {
      return false;
   }

   bool foundAny = false;
   std::string line;
   while (std::getline(statusFile, line))
   {
      std::istringstream parser(line);
      std::string key;
      parser >> key;

      if (key == "VmRSS:")
      {
         int64_t valueKb = 0;
         parser >> valueKb;
         rssBytes = valueKb * 1024;
         foundAny = true;
      }
      else if (key == "VmSize:")
      {
         int64_t valueKb = 0;
         parser >> valueKb;
         vmsBytes = valueKb * 1024;
         foundAny = true;
      }
      else if (key == "Threads:")
      {
         parser >> threadCount;
         foundAny = true;
      }
   }

   return foundAny;
}

class ProcessMetricsRuntime
{
public:
   ProcessMetricsRuntime() = default;

   ~ProcessMetricsRuntime()
   {
      stopSamplerThread();
      unregisterCallbacks();
   }

   void configure(const MetricsConfig &config)
   {
      _enabled.store(config.processMetricsEnabled());
      _samplingPeriodMs.store(config.processSamplingPeriod().count());

      _cpuGaugeEnabled.store(config.processGaugeCpuUtilizationEnabled());
      _rssGaugeEnabled.store(config.processGaugeMemoryRssEnabled());
      _vmsGaugeEnabled.store(config.processGaugeMemoryVmsEnabled());
      _threadGaugeEnabled.store(config.processGaugeThreadCountEnabled());

      if (!_enabled.load())
      {
         stopSamplerThread();
         unregisterCallbacks();
         return;
      }

      ensureGauges();
      refreshCallbackRegistration();
      restartSamplerThread();
   }

private:
   static long detectClockTicksPerSecond()
   {
      const long ticks = sysconf(_SC_CLK_TCK);
      return ticks > 0 ? ticks : 100;
   }

   void ensureGauges()
   {
      std::lock_guard<std::mutex> lock(_instrumentMutex);

      if (_cpuGauge == nullptr)
      {
         _cpuGauge = getConfiguredMeter()->CreateDoubleObservableGauge(
            "process.cpu.utilization.pct", "Process CPU utilization percentage", "%");
      }
      if (_rssGauge == nullptr)
      {
         _rssGauge = getConfiguredMeter()->CreateInt64ObservableGauge(
            "process.memory.rss.bytes", "Process resident memory usage", "By");
      }
      if (_vmsGauge == nullptr)
      {
         _vmsGauge = getConfiguredMeter()->CreateInt64ObservableGauge(
            "process.memory.vms.bytes", "Process virtual memory usage", "By");
      }
      if (_threadGauge == nullptr)
      {
         _threadGauge = getConfiguredMeter()->CreateInt64ObservableGauge(
            "process.thread.count", "Process thread count", "{thread}");
      }
   }

   void refreshCallbackRegistration()
   {
      std::lock_guard<std::mutex> lock(_instrumentMutex);

      if (_cpuGaugeEnabled.load() && !_cpuCallbackRegistered && _cpuGauge != nullptr)
      {
         _cpuGauge->AddCallback(&ProcessMetricsRuntime::observeCpuGauge, this);
         _cpuCallbackRegistered = true;
      }
      if (!_cpuGaugeEnabled.load() && _cpuCallbackRegistered && _cpuGauge != nullptr)
      {
         _cpuGauge->RemoveCallback(&ProcessMetricsRuntime::observeCpuGauge, this);
         _cpuCallbackRegistered = false;
      }

      if (_rssGaugeEnabled.load() && !_rssCallbackRegistered && _rssGauge != nullptr)
      {
         _rssGauge->AddCallback(&ProcessMetricsRuntime::observeRssGauge, this);
         _rssCallbackRegistered = true;
      }
      if (!_rssGaugeEnabled.load() && _rssCallbackRegistered && _rssGauge != nullptr)
      {
         _rssGauge->RemoveCallback(&ProcessMetricsRuntime::observeRssGauge, this);
         _rssCallbackRegistered = false;
      }

      if (_vmsGaugeEnabled.load() && !_vmsCallbackRegistered && _vmsGauge != nullptr)
      {
         _vmsGauge->AddCallback(&ProcessMetricsRuntime::observeVmsGauge, this);
         _vmsCallbackRegistered = true;
      }
      if (!_vmsGaugeEnabled.load() && _vmsCallbackRegistered && _vmsGauge != nullptr)
      {
         _vmsGauge->RemoveCallback(&ProcessMetricsRuntime::observeVmsGauge, this);
         _vmsCallbackRegistered = false;
      }

      if (_threadGaugeEnabled.load() && !_threadCallbackRegistered && _threadGauge != nullptr)
      {
         _threadGauge->AddCallback(&ProcessMetricsRuntime::observeThreadGauge, this);
         _threadCallbackRegistered = true;
      }
      if (!_threadGaugeEnabled.load() && _threadCallbackRegistered && _threadGauge != nullptr)
      {
         _threadGauge->RemoveCallback(&ProcessMetricsRuntime::observeThreadGauge, this);
         _threadCallbackRegistered = false;
      }
   }

   void unregisterCallbacks()
   {
      std::lock_guard<std::mutex> lock(_instrumentMutex);

      if (_cpuGauge != nullptr && _cpuCallbackRegistered)
      {
         _cpuGauge->RemoveCallback(&ProcessMetricsRuntime::observeCpuGauge, this);
         _cpuCallbackRegistered = false;
      }
      if (_rssGauge != nullptr && _rssCallbackRegistered)
      {
         _rssGauge->RemoveCallback(&ProcessMetricsRuntime::observeRssGauge, this);
         _rssCallbackRegistered = false;
      }
      if (_vmsGauge != nullptr && _vmsCallbackRegistered)
      {
         _vmsGauge->RemoveCallback(&ProcessMetricsRuntime::observeVmsGauge, this);
         _vmsCallbackRegistered = false;
      }
      if (_threadGauge != nullptr && _threadCallbackRegistered)
      {
         _threadGauge->RemoveCallback(&ProcessMetricsRuntime::observeThreadGauge, this);
         _threadCallbackRegistered = false;
      }
   }

   void restartSamplerThread()
   {
      stopSamplerThread();

      _hasCpuBaseline = false;
      _stopRequested.store(false);
      _samplerThread = std::thread(&ProcessMetricsRuntime::samplingLoop, this);
   }

   void stopSamplerThread()
   {
      _stopRequested.store(true);
      if (_samplerThread.joinable())
      {
         _samplerThread.join();
      }
   }

   void samplingLoop()
   {
      while (!_stopRequested.load())
      {
         collectSample();

         const auto waitMs = _samplingPeriodMs.load();
         const auto sleepMs = waitMs > 0 ? waitMs : 1000;
         std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
      }
   }

   void collectSample()
   {
      ProcessSnapshot snapshot;

      uint64_t processJiffies = 0;
      int64_t threadCountFromStat = 0;
      const bool statOk = readProcessStat(processJiffies, threadCountFromStat);

      int64_t rssBytes = _rssBytes.load();
      int64_t vmsBytes = _vmsBytes.load();
      int64_t threadCountFromStatus = _threadCount.load();
      const bool statusOk = readProcessStatusMemory(rssBytes, vmsBytes, threadCountFromStatus);

      snapshot.memoryRssBytes = statusOk ? rssBytes : _rssBytes.load();
      snapshot.memoryVmsBytes = statusOk ? vmsBytes : _vmsBytes.load();

      if (statOk)
      {
         snapshot.threadCount = threadCountFromStat;

         const auto now = std::chrono::steady_clock::now();
         if (_hasCpuBaseline)
         {
            const uint64_t deltaJiffies = processJiffies - _lastProcessJiffies;
            const auto deltaSeconds = std::chrono::duration<double>(now - _lastSampleTime).count();
            if (deltaSeconds > 0.0)
            {
               double cpu = (static_cast<double>(deltaJiffies) / static_cast<double>(_ticksPerSecond));
               cpu = (cpu / deltaSeconds) * 100.0;
               cpu = cpu / _cpuCoreCount;
               snapshot.cpuUtilizationPercent = cpu >= 0.0 ? cpu : 0.0;
            }
            else
            {
               snapshot.cpuUtilizationPercent = _cpuUtilizationPercent.load();
            }
         }
         else
         {
            snapshot.cpuUtilizationPercent = _cpuUtilizationPercent.load();
         }

         _lastProcessJiffies = processJiffies;
         _lastSampleTime = now;
         _hasCpuBaseline = true;
      }
      else
      {
         snapshot.cpuUtilizationPercent = _cpuUtilizationPercent.load();
         snapshot.threadCount = threadCountFromStatus;
      }

      _cpuUtilizationPercent.store(snapshot.cpuUtilizationPercent);
      _rssBytes.store(snapshot.memoryRssBytes);
      _vmsBytes.store(snapshot.memoryVmsBytes);
      _threadCount.store(snapshot.threadCount);
   }

   static void observeCpuGauge(opentelemetry::metrics::ObserverResult result, void *state)
   {
      auto *runtime = static_cast<ProcessMetricsRuntime *>(state);
      if (runtime == nullptr)
      {
         return;
      }

      if (auto typed = opentelemetry::nostd::get_if<
             opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(&result))
      {
         (*typed)->Observe(runtime->_cpuUtilizationPercent.load());
      }
   }

   static void observeRssGauge(opentelemetry::metrics::ObserverResult result, void *state)
   {
      auto *runtime = static_cast<ProcessMetricsRuntime *>(state);
      if (runtime == nullptr)
      {
         return;
      }

      if (auto typed = opentelemetry::nostd::get_if<
             opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(&result))
      {
         (*typed)->Observe(runtime->_rssBytes.load());
      }
   }

   static void observeVmsGauge(opentelemetry::metrics::ObserverResult result, void *state)
   {
      auto *runtime = static_cast<ProcessMetricsRuntime *>(state);
      if (runtime == nullptr)
      {
         return;
      }

      if (auto typed = opentelemetry::nostd::get_if<
             opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(&result))
      {
         (*typed)->Observe(runtime->_vmsBytes.load());
      }
   }

   static void observeThreadGauge(opentelemetry::metrics::ObserverResult result, void *state)
   {
      auto *runtime = static_cast<ProcessMetricsRuntime *>(state);
      if (runtime == nullptr)
      {
         return;
      }

      if (auto typed = opentelemetry::nostd::get_if<
             opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(&result))
      {
         (*typed)->Observe(runtime->_threadCount.load());
      }
   }

   std::atomic<bool> _enabled{false};
   std::atomic<bool> _cpuGaugeEnabled{true};
   std::atomic<bool> _rssGaugeEnabled{true};
   std::atomic<bool> _vmsGaugeEnabled{true};
   std::atomic<bool> _threadGaugeEnabled{true};
   std::atomic<int64_t> _samplingPeriodMs{1000};

   std::atomic<double> _cpuUtilizationPercent{0.0};
   std::atomic<int64_t> _rssBytes{0};
   std::atomic<int64_t> _vmsBytes{0};
   std::atomic<int64_t> _threadCount{0};

   std::mutex _instrumentMutex;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _cpuGauge;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _rssGauge;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _vmsGauge;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _threadGauge;
   bool _cpuCallbackRegistered{false};
   bool _rssCallbackRegistered{false};
   bool _vmsCallbackRegistered{false};
   bool _threadCallbackRegistered{false};

   std::atomic<bool> _stopRequested{false};
   std::thread _samplerThread;

   uint64_t _lastProcessJiffies{0};
   std::chrono::steady_clock::time_point _lastSampleTime;
   bool _hasCpuBaseline{false};
   const long _ticksPerSecond{detectClockTicksPerSecond()};
   const double _cpuCoreCount{
      std::thread::hardware_concurrency() > 0U ? static_cast<double>(std::thread::hardware_concurrency()) : 1.0};
};

ProcessMetricsRuntime &processMetricsRuntime()
{
   static ProcessMetricsRuntime runtime;
   return runtime;
}

// ---- sysfs GPU helpers (AMD / Intel discrete; silent no-op when absent) ------

std::string discoverFirstGpuCard()
{
   for (int i = 0; i < 8; ++i)
   {
      const std::string cardPath = "/sys/class/drm/card" + std::to_string(i);
      std::ifstream f(cardPath + "/device/gpu_busy_percent");
      if (f.is_open())
      {
         return cardPath;
      }
   }
   return {};
}

bool readGpuBusyPercent(const std::string &cardPath, double &percent)
{
   std::ifstream f(cardPath + "/device/gpu_busy_percent");
   if (!f.is_open())
   {
      return false;
   }
   int value = 0;
   f >> value;
   if (f.fail())
   {
      return false;
   }
   percent = static_cast<double>(value);
   return true;
}

bool readGpuMemory(const std::string &cardPath, int64_t &usedBytes, int64_t &totalBytes)
{
   std::ifstream fu(cardPath + "/device/mem_info_vram_used");
   std::ifstream ft(cardPath + "/device/mem_info_vram_total");
   if (!fu.is_open() || !ft.is_open())
   {
      return false;
   }
   fu >> usedBytes;
   ft >> totalBytes;
   return !fu.fail() && !ft.fail();
}

class GpuMetricsRuntime
{
public:
   GpuMetricsRuntime() = default;

   ~GpuMetricsRuntime()
   {
      stopSamplerThread();
      unregisterCallbacks();
   }

   void configure(const MetricsConfig &config)
   {
      _enabled.store(config.gpuMetricsEnabled());
      _samplingPeriodMs.store(config.gpuSamplingPeriod().count());
      _utilizationEnabled.store(config.gpuGaugeUtilizationEnabled());
      _memoryUsedEnabled.store(config.gpuGaugeMemoryUsedEnabled());
      _memoryTotalEnabled.store(config.gpuGaugeMemoryTotalEnabled());

      if (!_enabled.load())
      {
         stopSamplerThread();
         unregisterCallbacks();
         return;
      }

      _cardPath = discoverFirstGpuCard();
      if (_cardPath.empty())
      {
         return; // no GPU found -- silently skip
      }

      ensureGauges();
      refreshCallbackRegistration();
      restartSamplerThread();
   }

private:
   void ensureGauges()
   {
      std::lock_guard<std::mutex> lock(_instrumentMutex);

      if (_utilizationGauge == nullptr)
      {
         _utilizationGauge = getConfiguredMeter()->CreateDoubleObservableGauge(
            "gpu.utilization.pct", "GPU busy utilization percentage", "%");
      }
      if (_memoryUsedGauge == nullptr)
      {
         _memoryUsedGauge = getConfiguredMeter()->CreateInt64ObservableGauge(
            "gpu.memory.used.bytes", "GPU video memory in use", "By");
      }
      if (_memoryTotalGauge == nullptr)
      {
         _memoryTotalGauge = getConfiguredMeter()->CreateInt64ObservableGauge(
            "gpu.memory.total.bytes", "Total GPU video memory", "By");
      }
   }

   void refreshCallbackRegistration()
   {
      std::lock_guard<std::mutex> lock(_instrumentMutex);

      if (_utilizationEnabled.load() && !_utilizationCallbackRegistered && _utilizationGauge != nullptr)
      {
         _utilizationGauge->AddCallback(&GpuMetricsRuntime::observeUtilizationGauge, this);
         _utilizationCallbackRegistered = true;
      }
      if (!_utilizationEnabled.load() && _utilizationCallbackRegistered && _utilizationGauge != nullptr)
      {
         _utilizationGauge->RemoveCallback(&GpuMetricsRuntime::observeUtilizationGauge, this);
         _utilizationCallbackRegistered = false;
      }

      if (_memoryUsedEnabled.load() && !_memoryUsedCallbackRegistered && _memoryUsedGauge != nullptr)
      {
         _memoryUsedGauge->AddCallback(&GpuMetricsRuntime::observeMemoryUsedGauge, this);
         _memoryUsedCallbackRegistered = true;
      }
      if (!_memoryUsedEnabled.load() && _memoryUsedCallbackRegistered && _memoryUsedGauge != nullptr)
      {
         _memoryUsedGauge->RemoveCallback(&GpuMetricsRuntime::observeMemoryUsedGauge, this);
         _memoryUsedCallbackRegistered = false;
      }

      if (_memoryTotalEnabled.load() && !_memoryTotalCallbackRegistered && _memoryTotalGauge != nullptr)
      {
         _memoryTotalGauge->AddCallback(&GpuMetricsRuntime::observeMemoryTotalGauge, this);
         _memoryTotalCallbackRegistered = true;
      }
      if (!_memoryTotalEnabled.load() && _memoryTotalCallbackRegistered && _memoryTotalGauge != nullptr)
      {
         _memoryTotalGauge->RemoveCallback(&GpuMetricsRuntime::observeMemoryTotalGauge, this);
         _memoryTotalCallbackRegistered = false;
      }
   }

   void unregisterCallbacks()
   {
      std::lock_guard<std::mutex> lock(_instrumentMutex);

      if (_utilizationGauge != nullptr && _utilizationCallbackRegistered)
      {
         _utilizationGauge->RemoveCallback(&GpuMetricsRuntime::observeUtilizationGauge, this);
         _utilizationCallbackRegistered = false;
      }
      if (_memoryUsedGauge != nullptr && _memoryUsedCallbackRegistered)
      {
         _memoryUsedGauge->RemoveCallback(&GpuMetricsRuntime::observeMemoryUsedGauge, this);
         _memoryUsedCallbackRegistered = false;
      }
      if (_memoryTotalGauge != nullptr && _memoryTotalCallbackRegistered)
      {
         _memoryTotalGauge->RemoveCallback(&GpuMetricsRuntime::observeMemoryTotalGauge, this);
         _memoryTotalCallbackRegistered = false;
      }
   }

   void restartSamplerThread()
   {
      stopSamplerThread();
      _stopRequested.store(false);
      _samplerThread = std::thread(&GpuMetricsRuntime::samplingLoop, this);
   }

   void stopSamplerThread()
   {
      _stopRequested.store(true);
      if (_samplerThread.joinable())
      {
         _samplerThread.join();
      }
   }

   void samplingLoop()
   {
      while (!_stopRequested.load())
      {
         collectSample();
         const auto waitMs = _samplingPeriodMs.load();
         const auto sleepMs = waitMs > 0 ? waitMs : 1000;
         std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
      }
   }

   void collectSample()
   {
      double utilization = _utilizationPct.load();
      if (readGpuBusyPercent(_cardPath, utilization))
      {
         _utilizationPct.store(utilization);
      }

      int64_t memUsed = _memoryUsedBytes.load();
      int64_t memTotal = _memoryTotalBytes.load();
      if (readGpuMemory(_cardPath, memUsed, memTotal))
      {
         _memoryUsedBytes.store(memUsed);
         _memoryTotalBytes.store(memTotal);
      }
   }

   static void observeUtilizationGauge(opentelemetry::metrics::ObserverResult result, void *state)
   {
      auto *runtime = static_cast<GpuMetricsRuntime *>(state);
      if (runtime == nullptr)
      {
         return;
      }
      if (auto typed = opentelemetry::nostd::get_if<
             opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(&result))
      {
         (*typed)->Observe(runtime->_utilizationPct.load());
      }
   }

   static void observeMemoryUsedGauge(opentelemetry::metrics::ObserverResult result, void *state)
   {
      auto *runtime = static_cast<GpuMetricsRuntime *>(state);
      if (runtime == nullptr)
      {
         return;
      }
      if (auto typed = opentelemetry::nostd::get_if<
             opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(&result))
      {
         (*typed)->Observe(runtime->_memoryUsedBytes.load());
      }
   }

   static void observeMemoryTotalGauge(opentelemetry::metrics::ObserverResult result, void *state)
   {
      auto *runtime = static_cast<GpuMetricsRuntime *>(state);
      if (runtime == nullptr)
      {
         return;
      }
      if (auto typed = opentelemetry::nostd::get_if<
             opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(&result))
      {
         (*typed)->Observe(runtime->_memoryTotalBytes.load());
      }
   }

   std::atomic<bool> _enabled{false};
   std::atomic<bool> _utilizationEnabled{true};
   std::atomic<bool> _memoryUsedEnabled{true};
   std::atomic<bool> _memoryTotalEnabled{true};
   std::atomic<int64_t> _samplingPeriodMs{1000};

   std::string _cardPath;

   std::atomic<double> _utilizationPct{0.0};
   std::atomic<int64_t> _memoryUsedBytes{0};
   std::atomic<int64_t> _memoryTotalBytes{0};

   std::mutex _instrumentMutex;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _utilizationGauge;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _memoryUsedGauge;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _memoryTotalGauge;
   bool _utilizationCallbackRegistered{false};
   bool _memoryUsedCallbackRegistered{false};
   bool _memoryTotalCallbackRegistered{false};

   std::atomic<bool> _stopRequested{false};
   std::thread _samplerThread;
};

GpuMetricsRuntime &gpuMetricsRuntime()
{
   static GpuMetricsRuntime runtime;
   return runtime;
}

void recordBytesPerSecondHistogram(opentelemetry::metrics::Histogram<double> &histogram,
                                   Direction direction,
                                   uint64_t bytes,
                                   const char *interfaceName,
                                   const char *type,
                                   const char *userAttribute)
{
   const std::string key = makeRateKey(direction, interfaceName, type, userAttribute);
   const auto now = std::chrono::steady_clock::now();

   std::lock_guard<std::mutex> lock(rateStateMutex());
   auto &state = rateStates()[key];
   if (!state.initialized)
   {
      state.initialized = true;
      state.lastSampleTime = now;
      return;
   }

   const auto elapsedSeconds = std::chrono::duration<double>(now - state.lastSampleTime).count();
   state.lastSampleTime = now;
   if (elapsedSeconds <= 0.0)
   {
      return;
   }

   const double bytesPerSecond = static_cast<double>(bytes) / elapsedSeconds;
   histogram.Record(bytesPerSecond,
                    {{"interface", interfaceName != nullptr ? interfaceName : ""},
                     {"type", type != nullptr ? type : ""},
                     {"user", userAttribute != nullptr ? userAttribute : ""}},
                    opentelemetry::context::Context{});
}

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> getConfiguredMeter()
{
   auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
   if (configuredMeterVersion().empty())
   {
      return provider->GetMeter(configuredMeterName());
   }
   return provider->GetMeter(configuredMeterName(), configuredMeterVersion());
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> createExporter(const MetricsConfig &config)
{
   namespace otlp = opentelemetry::exporter::otlp;
   namespace ostream_exporter = opentelemetry::exporter::metrics;

   switch (config.exporterKind())
   {
   case MetricsExporterKind::Grpc:
   {
      otlp::OtlpGrpcMetricExporterOptions options;
      options.endpoint = config.endpoint();
      options.timeout = config.exportTimeout();
      options.use_ssl_credentials = !config.insecure();
      for (const auto &[key, value] : config.headers())
      {
         options.metadata.emplace(key, value);
      }
      return otlp::OtlpGrpcMetricExporterFactory::Create(options);
   }
   case MetricsExporterKind::Http:
   {
      otlp::OtlpHttpMetricExporterOptions options;
      options.url = config.endpoint();
      options.content_type = otlp::HttpRequestContentType::kJson;
      options.use_json_name = true;
      options.timeout = config.exportTimeout();
      options.ssl_insecure_skip_verify = config.insecure();
      for (const auto &[key, value] : config.headers())
      {
         options.http_headers.emplace(key, value);
      }
      return otlp::OtlpHttpMetricExporterFactory::Create(options);
   }
   case MetricsExporterKind::Console:
   {
      return ostream_exporter::OStreamMetricExporterFactory::Create();
   }
   default:
   {
      return ostream_exporter::OStreamMetricExporterFactory::Create();
   }
   }
}

} // namespace

class OtelInterfaceMetrics : public IInterfaceMetrics
{
public:
   void recordSent(uint64_t messageCount, uint64_t bytes,
                   const char *interfaceName,
                   const char *type,
                   const char *userAttribute = "") override
   {
      record(Direction::Sent,
             messagesSentCounter(),
             bytesSentCounter(),
             bytesSentPerCallHistogram(),
             bytesSentPerSecondHistogram(),
             messageCount,
             bytes,
             interfaceName,
             type,
             userAttribute);
   }

   void recordReceived(uint64_t messageCount, uint64_t bytes,
                       const char *interfaceName,
                       const char *type,
                       const char *userAttribute = "") override
   {
      record(Direction::Received,
             messagesReceivedCounter(),
             bytesReceivedCounter(),
             bytesReceivedPerCallHistogram(),
             bytesReceivedPerSecondHistogram(),
             messageCount,
             bytes,
             interfaceName,
             type,
             userAttribute);
   }

private:
   static void record(Direction direction,
                      opentelemetry::metrics::Counter<uint64_t> &messageCounter,
                      opentelemetry::metrics::Counter<uint64_t> &byteCounter,
                      opentelemetry::metrics::Histogram<uint64_t> &bytesPerCallHistogram,
                      opentelemetry::metrics::Histogram<double> &bytesPerSecondHistogram,
                      uint64_t messageCount,
                      uint64_t bytes,
                      const char *interfaceName,
                      const char *type,
                      const char *userAttribute)
   {
      const AttributeList attributes = makeAttributes(interfaceName, type, userAttribute);
      messageCounter.Add(messageCount, attributes);
      byteCounter.Add(bytes, attributes);

      if (histogramToggles().bytesPerCallEnabled.load())
      {
         bytesPerCallHistogram.Record(bytes,
                                      {{"interface", interfaceName != nullptr ? interfaceName : ""},
                                       {"type", type != nullptr ? type : ""},
                                       {"user", userAttribute != nullptr ? userAttribute : ""}},
                                      opentelemetry::context::Context{});
      }
      if (histogramToggles().bytesPerSecondEnabled.load())
      {
         recordBytesPerSecondHistogram(bytesPerSecondHistogram,
                                       direction,
                                       bytes,
                                       interfaceName,
                                       type,
                                       userAttribute);
      }
   }

   using AttributeList = std::array<std::pair<opentelemetry::nostd::string_view, opentelemetry::nostd::string_view>, 3>;

   static AttributeList makeAttributes(const char *interfaceName,
                                       const char *type,
                                       const char *userAttribute)
   {
      return AttributeList{{
         std::make_pair(opentelemetry::nostd::string_view("interface"),
                        opentelemetry::nostd::string_view(interfaceName)),
         std::make_pair(opentelemetry::nostd::string_view("type"),
                        opentelemetry::nostd::string_view(type != nullptr ? type : "")),
         std::make_pair(opentelemetry::nostd::string_view("user"),
                        opentelemetry::nostd::string_view(userAttribute != nullptr ? userAttribute : ""))
      }};
   }

   static opentelemetry::metrics::Counter<uint64_t> &messagesSentCounter()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> counter =
         getConfiguredMeter()
            ->CreateUInt64Counter("interface.messages.sent", "Number of messages sent", "{message}");
      return *counter;
   }

   static opentelemetry::metrics::Counter<uint64_t> &messagesReceivedCounter()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> counter =
         getConfiguredMeter()
            ->CreateUInt64Counter("interface.messages.received", "Number of messages received", "{message}");
      return *counter;
   }

   static opentelemetry::metrics::Counter<uint64_t> &bytesSentCounter()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> counter =
         getConfiguredMeter()
            ->CreateUInt64Counter("interface.bytes.sent", "Number of bytes sent", "By");
      return *counter;
   }

   static opentelemetry::metrics::Counter<uint64_t> &bytesReceivedCounter()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> counter =
         getConfiguredMeter()
            ->CreateUInt64Counter("interface.bytes.received", "Number of bytes received", "By");
      return *counter;
   }

   static opentelemetry::metrics::Histogram<uint64_t> &bytesSentPerCallHistogram()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<uint64_t>> histogram =
         getConfiguredMeter()
            ->CreateUInt64Histogram("interface.bytes.sent.per_call", "Bytes sent per emit call", "By");
      return *histogram;
   }

   static opentelemetry::metrics::Histogram<uint64_t> &bytesReceivedPerCallHistogram()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<uint64_t>> histogram =
         getConfiguredMeter()
            ->CreateUInt64Histogram("interface.bytes.received.per_call", "Bytes received per emit call", "By");
      return *histogram;
   }

   static opentelemetry::metrics::Histogram<double> &bytesSentPerSecondHistogram()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>> histogram =
         getConfiguredMeter()
            ->CreateDoubleHistogram("interface.bytes.sent.per_second", "Sent throughput between emit calls", "By/s");
      return *histogram;
   }

   static opentelemetry::metrics::Histogram<double> &bytesReceivedPerSecondHistogram()
   {
      static opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>> histogram =
         getConfiguredMeter()
            ->CreateDoubleHistogram("interface.bytes.received.per_second", "Received throughput between emit calls", "By/s");
      return *histogram;
   }

};


std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> init(const MetricsConfig &config)
{
   namespace metrics_sdk = opentelemetry::sdk::metrics;
   namespace metrics_api = opentelemetry::metrics;
   namespace resource = opentelemetry::sdk::resource;

   configuredMeterName() = config.meterName();
   configuredMeterVersion() = config.meterVersion();

   histogramToggles().bytesPerCallEnabled.store(config.interfaceHistogramBytesPerCallEnabled());
   histogramToggles().bytesPerSecondEnabled.store(config.interfaceHistogramBytesPerSecondEnabled());
   resetRateStates();

   auto exporter = createExporter(config);

   metrics_sdk::PeriodicExportingMetricReaderOptions readerOptions;
   readerOptions.export_interval_millis = config.aggregationPeriod();
   readerOptions.export_timeout_millis = config.exportTimeout();
   auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), readerOptions);

   resource::ResourceAttributes resourceAttributes;
   resourceAttributes["service.name"] = config.serviceName();
   for (const auto &[key, value] : config.resourceAttributes())
   {
      resourceAttributes[key] = value;
   }

   auto context = metrics_sdk::MeterContextFactory::Create(
      metrics_sdk::ViewRegistryFactory::Create(),
      resource::Resource::Create(resourceAttributes));
   context->AddMetricReader(std::move(reader));

   // MeterProviderFactory::Create() returns the API base type by design, so
   // that library code stays decoupled from the concrete SDK. We keep an SDK-
   // typed alias (sharing the same control block) so main() can still call
   // the SDK-only ForceFlush()/Shutdown() on the same object before exit.
   std::shared_ptr<metrics_api::MeterProvider> apiProvider(metrics_sdk::MeterProviderFactory::Create(std::move(context)));
   auto sdkProvider = std::static_pointer_cast<metrics_sdk::MeterProvider>(apiProvider);

   metrics_api::Provider::SetMeterProvider(apiProvider);

   // Observable gauges must be created against the installed SDK provider, not
   // the default no-op provider, so configure them only after SetMeterProvider.
   processMetricsRuntime().configure(config);
   gpuMetricsRuntime().configure(config);

   return sdkProvider;
}

IInterfaceMetrics &metrics()
{
   static OtelInterfaceMetrics instance;
   return instance;
}

} // namespace Observability
