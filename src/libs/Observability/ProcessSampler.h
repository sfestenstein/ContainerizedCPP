#ifndef OBSERVABILITY_PROCESSSAMPLER_H_
#define OBSERVABILITY_PROCESSSAMPLER_H_

// System headers
#include <chrono>
#include <cstdint>

namespace Observability
{

/**
 * @brief One point-in-time sample of this process's CPU/memory usage.
 */
struct ProcessSample
{
   /// Fraction of one CPU core used since the previous sample() call, 0..1+.
   /// 0.0 on the first call, since there is no prior delta to compare against.
   double cpuUtilization;
   /// Current resident set size (RSS) in bytes, from /proc/self/status VmRSS.
   int64_t residentMemoryBytes;
};

/**
 * @brief Samples this process's CPU and memory usage from /proc.
 *
 * Pure logic, zero OTel dependency -- see ProcessMetrics.h for the OTel
 * observable-gauge wiring around this. Reads /proc/self/stat (utime+stime)
 * and /proc/self/status (VmRSS); Linux-only, consistent with the rest of
 * this container-based project.
 */
class ProcessSampler
{
public:
   ProcessSampler();

   /**
    * @brief Take a sample, computing cpuUtilization as a delta against the
    *        previous call.
    */
   ProcessSample sample();

private:
   long _clockTicksPerSec;
   uint64_t _previousCpuTicks = 0;
   std::chrono::steady_clock::time_point _previousSampleTime;
   bool _hasPreviousSample = false;
};

} // namespace Observability

#endif // OBSERVABILITY_PROCESSSAMPLER_H_
