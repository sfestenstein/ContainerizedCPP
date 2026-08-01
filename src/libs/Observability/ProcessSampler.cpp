#include "Observability/ProcessSampler.h"

#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>

namespace Observability
{

namespace
{

// /proc/[pid]/stat fields (see `man proc`), 1-indexed: 1=pid, 2=comm
// (parenthesized, may itself contain ')'), 3=state, 4=ppid, 5=pgrp,
// 6=session, 7=tty_nr, 8=tpgid, 9=flags, 10=minflt, 11=cminflt, 12=majflt,
// 13=cmajflt, 14=utime, 15=stime. Skip to the last ')' to get past comm
// safely, then skip 11 more whitespace-separated tokens (state..cmajflt)
// to reach utime/stime.
uint64_t readCpuTicks()
{
   std::ifstream stat("/proc/self/stat");
   std::string line;
   if (!stat || !std::getline(stat, line))
   {
      return 0;
   }

   auto commEnd = line.rfind(')');
   if (commEnd == std::string::npos)
   {
      return 0;
   }

   std::istringstream rest(line.substr(commEnd + 1));
   std::string token;
   for (int field = 0; field < 11; ++field)
   {
      if (!(rest >> token))
      {
         return 0;
      }
   }

   uint64_t utime = 0;
   uint64_t stime = 0;
   if (!(rest >> utime) || !(rest >> stime))
   {
      return 0;
   }

   return utime + stime;
}

int64_t readResidentMemoryBytes()
{
   std::ifstream status("/proc/self/status");
   std::string line;
   while (std::getline(status, line))
   {
      if (line.rfind("VmRSS:", 0) == 0)
      {
         std::istringstream iss(line.substr(6));
         int64_t kilobytes = 0;
         if (iss >> kilobytes)
         {
            return kilobytes * 1024;
         }
         return 0;
      }
   }
   return 0;
}

} // namespace

ProcessSampler::ProcessSampler() : _clockTicksPerSec(sysconf(_SC_CLK_TCK))
{
}

ProcessSample ProcessSampler::sample()
{
   ProcessSample result{0.0, readResidentMemoryBytes()};

   uint64_t cpuTicks = readCpuTicks();
   auto now = std::chrono::steady_clock::now();

   if (_hasPreviousSample && _clockTicksPerSec > 0)
   {
      uint64_t tickDelta = cpuTicks - _previousCpuTicks;
      double wallSeconds = std::chrono::duration<double>(now - _previousSampleTime).count();
      if (wallSeconds > 0.0)
      {
         double cpuSeconds = static_cast<double>(tickDelta) / static_cast<double>(_clockTicksPerSec);
         result.cpuUtilization = cpuSeconds / wallSeconds;
      }
   }

   _previousCpuTicks = cpuTicks;
   _previousSampleTime = now;
   _hasPreviousSample = true;

   return result;
}

} // namespace Observability
