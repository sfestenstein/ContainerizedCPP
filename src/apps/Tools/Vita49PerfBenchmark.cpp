// =============================================================================
// Vita49PerfBenchmark
// =============================================================================
// Measures VITA 49.2 encode and decode throughput at various sample sizes.
// Reports samples/sec, MB/sec, and packets/sec.
//
// Usage: ./Vita49PerfBenchmark [iterations]
//        iterations - Number of timing iterations per test (default: 100)
// =============================================================================

#include "GeneralLogger.h"
#include "Vita49Codec.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace
{

// ============================================================================
// Helpers
// ============================================================================

/// Generate random I/Q signal (amplitude 0.8 to stay within int16 range)
Vita49_2::IQSamples generateTestSignal(size_t count, uint32_t seed = 12345)
{
   Vita49_2::IQSamples samples(count);
   std::mt19937 gen(seed);
   std::uniform_real_distribution<float> dist(-0.8f, 0.8f);

   for (size_t i = 0; i < count; ++i)
   {
      samples[i] = {dist(gen), dist(gen)};
   }
   return samples;
}

struct BenchResult
{
   double encodeMs;          ///< Total encode time in ms
   double decodeMs;          ///< Total decode time in ms
   size_t totalSamples;      ///< Total samples processed
   size_t totalBytes;        ///< Total encoded bytes
   size_t totalPackets;      ///< Total packets produced
   int iterations;
};

void logHeader()
{
   GPINFO("{:<14s}{:<12s}{:<14s}{:<14s}{:<12s}{:<14s}{:<14s}{:<10s}",
          "Samples", "Enc(ms)", "Enc(MSa/s)", "Enc(MB/s)",
          "Dec(ms)", "Dec(MSa/s)", "Dec(MB/s)", "Packets");
   GPINFO("{}", std::string(100, '-'));
}

void logResult(size_t sampleCount, const BenchResult& r)
{
   double avgEncMs = r.encodeMs / r.iterations;
   double avgDecMs = r.decodeMs / r.iterations;

   double encMSaPerSec = (static_cast<double>(r.totalSamples) / r.iterations) /
                          (avgEncMs * 1000.0);   // mega-samples/sec
   double decMSaPerSec = (static_cast<double>(r.totalSamples) / r.iterations) /
                          (avgDecMs * 1000.0);

   double encMBps = (static_cast<double>(r.totalBytes) / r.iterations) /
                    (avgEncMs * 1000.0);   // MB/sec
   double decMBps = (static_cast<double>(r.totalBytes) / r.iterations) /
                    (avgDecMs * 1000.0);

   size_t pktsPerIter = r.totalPackets / static_cast<size_t>(r.iterations);

   GPINFO("{:<14}{:<12.3f}{:<14.2f}{:<14.2f}{:<12.3f}{:<14.2f}{:<14.2f}{:<10}",
          sampleCount, avgEncMs, encMSaPerSec, encMBps,
          avgDecMs, decMSaPerSec, decMBps, pktsPerIter);
}

BenchResult runBenchmark(Vita49_2::Vita49Codec& codec,
                         const Vita49_2::IQSamples& samples,
                         uint32_t streamId,
                         int iterations)
{
   BenchResult result{};
   result.iterations    = iterations;
   result.totalSamples  = samples.size() * static_cast<size_t>(iterations);

   // ----- Encode benchmark -----
   std::vector<std::vector<uint8_t>> encodedBuffers;
   encodedBuffers.reserve(static_cast<size_t>(iterations));

   auto encStart = std::chrono::steady_clock::now();
   for (int i = 0; i < iterations; ++i)
   {
      encodedBuffers.push_back(codec.encodeSignalData(streamId, samples));
   }
   auto encEnd = std::chrono::steady_clock::now();
   result.encodeMs = std::chrono::duration<double, std::milli>(encEnd - encStart).count();

   // Compute total bytes
   for (const auto& buf : encodedBuffers)
   {
      result.totalBytes += buf.size();
   }

   // ----- Decode benchmark -----
   auto decStart = std::chrono::steady_clock::now();
   for (int i = 0; i < iterations; ++i)
   {
      auto packets = codec.parseStream(
         encodedBuffers[static_cast<size_t>(i)].data(),
                                       encodedBuffers[static_cast<size_t>(i)].size());
      result.totalPackets += packets.size();
   }
   auto decEnd = std::chrono::steady_clock::now();
   result.decodeMs = std::chrono::duration<double, std::milli>(decEnd - decStart).count();

   return result;
}

BenchResult runContextBenchmark(Vita49_2::Vita49Codec& codec,
                                uint32_t streamId,
                                int iterations)
{
   // Build a context with all supported fields
   Vita49_2::ContextFields fields;
   fields.changeIndicator = true;
   fields.referencePointId = 0x00ABCDEF;
   fields.bandwidth      = 20.0e6;
   fields.ifRefFrequency = 70.0e6;
   fields.rfFrequency    = 2.4e9;
   fields.rfFreqOffset   = 100.0e3;
   fields.ifBandOffset   = -5.0e3;
   fields.referenceLevel = -30.5;
   fields.gain           = 15.25;
   fields.overRangeCount = 42;
   fields.sampleRate     = 1.0e6;

   BenchResult result{};
   result.iterations = iterations;
   result.totalSamples = 0;

   // Encode
   std::vector<std::vector<uint8_t>> encodedBuffers;
   encodedBuffers.reserve(static_cast<size_t>(iterations));

   auto encStart = std::chrono::steady_clock::now();
   for (int i = 0; i < iterations; ++i)
   {
      encodedBuffers.push_back(codec.encodeContext(streamId, fields));
   }
   auto encEnd = std::chrono::steady_clock::now();
   result.encodeMs = std::chrono::duration<double, std::milli>(encEnd - encStart).count();

   for (const auto& buf : encodedBuffers)
   {
      result.totalBytes += buf.size();
   }

   // Decode
   auto decStart = std::chrono::steady_clock::now();
   for (int i = 0; i < iterations; ++i)
   {
      auto packets = codec.parseStream(
         encodedBuffers[static_cast<size_t>(i)].data(),
                                       encodedBuffers[static_cast<size_t>(i)].size());
      result.totalPackets += packets.size();
   }
   auto decEnd = std::chrono::steady_clock::now();
   result.decodeMs = std::chrono::duration<double, std::milli>(decEnd - decStart).count();

   return result;
}

} // anonymous namespace

// ============================================================================
// main
// ============================================================================
// NOLINTNEXTLINE
int main(int argc, char* argv[])
{
   CommonUtils::GeneralLogger logger;
   logger.init("Vita49PerfBenchmark");

   int iterations = 100;
   if (argc > 1) { iterations = std::atoi(argv[1]); }
   iterations = std::max(iterations, 1);

   constexpr uint32_t STREAM_ID = 0xBEEF;

   GPINFO("==========================================================");
   GPINFO("VITA 49.2 Performance Benchmark");
   GPINFO("==========================================================");
   GPINFO("  Iterations per size: {}", iterations);
   GPINFO("==========================================================");

   // ----- Signal Data — BigEndian -----
   GPINFO("[Signal Data — BigEndian]");
   {
      Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::BigEndian);
      logHeader();

      for (const size_t sz : {100UL, 1000UL, 10000UL, 50000UL, 100000UL, 500000UL, 1000000UL})
      {
         auto samples = generateTestSignal(sz);
         auto result  = runBenchmark(codec, samples, STREAM_ID, iterations);
         logResult(sz, result);
      }
   }

   // ----- Signal Data — LittleEndian -----
   GPINFO("[Signal Data — LittleEndian]");
   {
      Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::LittleEndian);
      logHeader();

      for (const size_t sz : {100UL, 1000UL, 10000UL, 100000UL, 1000000UL})
      {
         auto samples = generateTestSignal(sz);
         auto result  = runBenchmark(codec, samples, STREAM_ID, iterations);
         logResult(sz, result);
      }
   }

   // ----- Context Packets -----
   GPINFO("[Context Packets — BigEndian, all fields]");
   {
      Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::BigEndian);

      auto result = runContextBenchmark(codec, STREAM_ID, iterations * 10);

      double avgEncUs = (result.encodeMs / result.iterations) * 1000.0;
      double avgDecUs = (result.decodeMs / result.iterations) * 1000.0;

      GPINFO("  Encode: {:.2f} us/packet ({:.0f} bytes)",
             avgEncUs, static_cast<double>(result.totalBytes) / result.iterations);
      GPINFO("  Decode: {:.2f} us/packet", avgDecUs);
   }

   GPINFO("==========================================================");
   GPINFO("Benchmark complete.");
   GPINFO("==========================================================");

   return 0;
}
