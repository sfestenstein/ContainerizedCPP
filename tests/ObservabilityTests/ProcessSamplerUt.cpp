/**
 * @file ProcessSamplerUt.cpp
 * @brief Unit tests for ProcessSampler.
 *
 * ProcessSampler has zero OTel dependency (pure /proc parsing), so these
 * tests need no fake/mock and no OTel SDK setup.
 */

#include <gtest/gtest.h>
#include "Observability/ProcessSampler.h"

#include <unistd.h>

#include <chrono>
#include <thread>

namespace
{

long coreCount()
{
   long n = sysconf(_SC_NPROCESSORS_ONLN);
   return n > 0 ? n : 1;
}

} // namespace

TEST(ProcessSamplerTest, FirstSample_HasNoPriorDelta_ReportsZeroCpuUtilization)
{
   Observability::ProcessSampler sampler;

   auto sample = sampler.sample();

   EXPECT_DOUBLE_EQ(sample.cpuUtilization, 0.0);
   EXPECT_GT(sample.residentMemoryBytes, 0);
}

TEST(ProcessSamplerTest, SecondSample_ComputesDelta_WithinSaneRange)
{
   Observability::ProcessSampler sampler;
   sampler.sample(); // establish a baseline; first call is always 0.0

   std::this_thread::sleep_for(std::chrono::milliseconds(200));

   auto sample = sampler.sample();

   EXPECT_GE(sample.cpuUtilization, 0.0);
   EXPECT_LE(sample.cpuUtilization, static_cast<double>(coreCount()));
   EXPECT_GT(sample.residentMemoryBytes, 0);
}

TEST(ProcessSamplerTest, ResidentMemory_StaysPositive_AcrossMultipleSamples)
{
   Observability::ProcessSampler sampler;

   for (int i = 0; i < 3; ++i)
   {
      auto sample = sampler.sample();
      EXPECT_GT(sample.residentMemoryBytes, 0);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }
}
