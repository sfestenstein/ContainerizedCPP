// =============================================================================
// Vita49RoundTripTest
// =============================================================================
// Generates known waveforms (tone, chirp, noise), encodes them as VITA 49.2
// packets, decodes them back, and reports accuracy metrics.
//
// Usage: ./Vita49RoundTripTest [numSamples] [scaleFactor]
//        numSamples   - Number of I/Q samples per waveform (default: 10000)
//        scaleFactor  - int16/float scale factor (default: 32768.0)
// =============================================================================

#include "GeneralLogger.h"
#include "Vita49Codec.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <random>
#include <string>
#include <vector>

namespace
{

// ============================================================================
// Waveform generators
// ============================================================================

/// Pure tone: I = cos(2πft/fs), Q = sin(2πft/fs)
Vita49_2::IQSamples generateTone(size_t count, double freqHz, double sampleRateHz)
{
   Vita49_2::IQSamples samples(count);
   for (size_t i = 0; i < count; ++i)
   {
      const double t = static_cast<double>(i) / sampleRateHz;
      auto phase = static_cast<float>(2.0 * std::numbers::pi * freqHz * t);
      samples[i] = {std::cos(phase), std::sin(phase)};
   }
   return samples;
}

/// Linear chirp: frequency sweeps from f0 to f1 over the duration
Vita49_2::IQSamples generateChirp(size_t count, double f0, double f1, double sampleRateHz)
{
   Vita49_2::IQSamples samples(count);
   const double duration = static_cast<double>(count) / sampleRateHz;
   const double rate = (f1 - f0) / duration;

   for (size_t i = 0; i < count; ++i)
   {
      const double t = static_cast<double>(i) / sampleRateHz;
      auto phase = static_cast<float>(2.0 * std::numbers::pi * (f0 * t + 0.5 * rate * t * t));
      samples[i] = {std::cos(phase), std::sin(phase)};
   }
   return samples;
}

/// Gaussian noise with specified standard deviation (amplitude 0..~3σ)
Vita49_2::IQSamples generateNoise(size_t count, float stddev, uint32_t seed)
{
   Vita49_2::IQSamples samples(count);
   std::mt19937 gen(seed);
   std::normal_distribution<float> dist(0.0f, stddev);

   for (size_t i = 0; i < count; ++i)
   {
      samples[i] = {dist(gen), dist(gen)};
   }
   return samples;
}

/// Scaled tone — amplitude < 1.0 to avoid clipping at full scale
Vita49_2::IQSamples generateScaledTone(size_t count, double freqHz,
                                       double sampleRateHz, float amplitude)
{
   Vita49_2::IQSamples samples(count);
   for (size_t i = 0; i < count; ++i)
   {
      const double t = static_cast<double>(i) / sampleRateHz;
      auto phase = static_cast<float>(2.0 * std::numbers::pi * freqHz * t);
      samples[i] = {amplitude * std::cos(phase), amplitude * std::sin(phase)};
   }
   return samples;
}

// ============================================================================
// Error metrics
// ============================================================================

struct ErrorMetrics
{
   float maxAbsError;    ///< Max |original - decoded| across all I and Q values
   float rmsError;       ///< RMS error across all I and Q values
   float snrDb;          ///< Signal-to-noise ratio in dB
   size_t sampleCount;
};

ErrorMetrics computeMetrics(const Vita49_2::IQSamples& original,
                            const Vita49_2::IQSamples& decoded)
{
   ErrorMetrics metrics{};
   metrics.sampleCount = original.size();

   if (original.size() != decoded.size())
   {
      GPWARN("Sample count mismatch — original={} decoded={}",
             original.size(), decoded.size());
      metrics.sampleCount = std::min(original.size(), decoded.size());
   }

   double sumSqError = 0.0;
   double sumSqSignal = 0.0;
   float maxErr = 0.0f;

   for (size_t i = 0; i < metrics.sampleCount; ++i)
   {
      const float errI = original[i].real() - decoded[i].real();
      const float errQ = original[i].imag() - decoded[i].imag();

      const float absErrI = std::fabs(errI);
      const float absErrQ = std::fabs(errQ);
      maxErr = std::max({maxErr, absErrI, absErrQ});

      sumSqError  += static_cast<double>((errI * errI) + (errQ * errQ));
      sumSqSignal += static_cast<double>((original[i].real() * original[i].real()) +
                                         (original[i].imag() * original[i].imag()));
   }

   const double n = static_cast<double>(metrics.sampleCount) * 2.0;   // I+Q values
   metrics.maxAbsError = maxErr;
   metrics.rmsError = static_cast<float>(std::sqrt(sumSqError / n));

   if (sumSqError > 0.0)
   {
      metrics.snrDb = static_cast<float>(10.0 * std::log10(sumSqSignal / sumSqError));
   }
   else
   {
      metrics.snrDb = std::numeric_limits<float>::infinity();
   }

   return metrics;
}

// ============================================================================
// Report
// ============================================================================

void logMetrics(const std::string& name, const ErrorMetrics& m)
{
   GPINFO("  {:24s} samples={:<8} maxErr={:.4e} rmsErr={:.4e} SNR={:.2f} dB",
          name, m.sampleCount, m.maxAbsError, m.rmsError, m.snrDb);
}

bool runRoundTrip(const std::string& name,
                  const Vita49_2::IQSamples& original,
                  Vita49_2::Vita49Codec& codec,
                  uint32_t streamId,
                  float snrThresholdDb)
{
   // Encode
   auto encoded = codec.encodeSignalData(streamId, original);
   if (encoded.empty() && !original.empty())
   {
      GPERROR("FAIL: {} — encode returned empty", name);
      return false;
   }

   // Decode
   auto packets = codec.parseStream(encoded.data(), encoded.size());
   if (packets.empty() && !original.empty())
   {
      GPERROR("FAIL: {} — decode returned no packets", name);
      return false;
   }

   // Reassemble all samples from decoded packets
   Vita49_2::IQSamples decoded;
   for (const auto& pkt : packets)
   {
      if (pkt.type == Vita49_2::ParsedPacket::Type::SignalData)
      {
         decoded.insert(decoded.end(), pkt.samples.begin(), pkt.samples.end());
      }
   }

   auto metrics = computeMetrics(original, decoded);
   logMetrics(name, metrics);

   if (metrics.snrDb < snrThresholdDb)
   {
      GPERROR("FAIL: {} — SNR {:.2f} dB below threshold {:.2f} dB",
              name, metrics.snrDb, snrThresholdDb);
      return false;
   }

   return true;
}

} // anonymous namespace

// ============================================================================
// main
// ============================================================================
// NOLINTNEXTLINE
int main(int argc, char* argv[])
{
   CommonUtils::GeneralLogger logger;
   logger.init("Vita49RoundTripTest");

   size_t numSamples = 10000;
   float scaleFactor = Vita49_2::DEFAULT_SCALE_FACTOR;

   if (argc > 1) { numSamples = static_cast<size_t>(std::atol(argv[1])); }
   if (argc > 2) { scaleFactor = std::stof(argv[2]); }

   GPINFO("==========================================================");
   GPINFO("VITA 49.2 Round-Trip Accuracy Test");
   GPINFO("==========================================================");
   GPINFO("  Samples per waveform: {}", numSamples);
   GPINFO("  Scale factor:         {}", scaleFactor);
   GPINFO("==========================================================");

   // 16-bit quantization gives ~96 dB theoretical max SNR for full-scale
   // signals, but practical limit is ~90 dB. Use conservative threshold.
   constexpr float SNR_THRESHOLD_DB = 80.0f;
   constexpr double SAMPLE_RATE     = 1.0e6;
   constexpr uint32_t STREAM_ID     = 0x1234;

   int failures = 0;

   // ----- BigEndian tests -----
   GPINFO("[BigEndian]");
   {
      Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::BigEndian, scaleFactor);

      if (!runRoundTrip("Tone (1 kHz)",
                        generateScaledTone(numSamples, 1000.0, SAMPLE_RATE, 0.9f),
                        codec, STREAM_ID, SNR_THRESHOLD_DB))
      {
         ++failures;
      }

      if (!runRoundTrip("Tone (100 kHz)",
                        generateScaledTone(numSamples, 100000.0, SAMPLE_RATE, 0.5f), 
                        codec, STREAM_ID, SNR_THRESHOLD_DB))
      {
         ++failures;
      }

      if (!runRoundTrip("Chirp (1k-100k)",
            generateChirp(numSamples, 1000.0, 100000.0, SAMPLE_RATE),
            codec, STREAM_ID, SNR_THRESHOLD_DB))
      {
         ++failures;
      }

      // Noise: lower SNR threshold since signal power is low
      if (!runRoundTrip("Gaussian noise (s=0.3)",
            generateNoise(numSamples, 0.3f, 42),
            codec, STREAM_ID, 40.0f))
      {
         ++failures;
      }

      if (!runRoundTrip("Full-scale tone",
            generateTone(numSamples, 10000.0, SAMPLE_RATE),
            codec, STREAM_ID, 70.0f))
      {
            ++failures;
      }
   }

   // ----- LittleEndian tests -----
   GPINFO("[LittleEndian]");
   {
      Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::LittleEndian, scaleFactor);

      if (!runRoundTrip("Tone (1 kHz)",
            generateScaledTone(numSamples, 1000.0, SAMPLE_RATE, 0.9f),
            codec, STREAM_ID, SNR_THRESHOLD_DB))
      {
         ++failures;
      }

      if (!runRoundTrip("Chirp (1k-100k)",
            generateChirp(numSamples, 1000.0, 100000.0, SAMPLE_RATE),
            codec, STREAM_ID, SNR_THRESHOLD_DB))
      {
         ++failures;
      }
   }

   // ----- Context round-trip -----
   GPINFO("[Context Packet Round-Trip]");
   {
      const Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::BigEndian, scaleFactor);

      Vita49_2::ContextFields original;
      original.changeIndicator = true;
      original.referencePointId = 0x00ABCDEF;
      original.bandwidth      = 20.0e6;
      original.ifRefFrequency = 70.0e6;
      original.rfFrequency    = 2.4e9;
      original.rfFreqOffset   = 100.0e3;
      original.ifBandOffset   = -5.0e3;
      original.referenceLevel = -30.5;
      original.gain           = 15.25;
      original.overRangeCount = 42;
      original.sampleRate     = 1.0e6;

      auto encoded = codec.encodeContext(STREAM_ID, original);
      auto packets = codec.parseStream(encoded.data(), encoded.size());

      if (packets.size() != 1 ||
          packets[0].type != Vita49_2::ParsedPacket::Type::Context)
      {
         GPERROR("FAIL: Context round-trip — unexpected packet count or type");
         ++failures;
      }
      else
      {
         const auto& f = packets[0].contextFields;
         bool pass = true;

         // Frequency fields: 20 fractional bits → ~1 µHz precision
         auto checkFreq = [&](const char* fieldName,
                              std::optional<double> orig,
                              std::optional<double> dec)
         {
            if (orig.has_value() != dec.has_value())
            {
               GPERROR("FAIL: Context.{} presence mismatch", fieldName);
               pass = false;
               return;
            }
            if (orig.has_value())
            {
               const double err = std::fabs(*orig - *dec);
               const double tol = (std::fabs(*orig) / Vita49_2::FREQ_RADIX) + 1.0e-6;
               if (err > tol)
               {
                  GPERROR("FAIL: Context.{} error={} > tol={}", fieldName, err, tol);
                  pass = false;
               }
            }
         };

         // Gain/level fields: 7 fractional bits → ~0.0078 dB precision
         auto checkGain = [&](const char* fieldName,
                              std::optional<double> orig,
                              std::optional<double> dec)
         {
            if (orig.has_value() != dec.has_value())
            {
               GPERROR("FAIL: Context.{} presence mismatch", fieldName);
               pass = false;
               return;
            }
            if (orig.has_value())
            {
               const double err = std::fabs(*orig - *dec);
               if (err > 0.01)
               {
                  GPERROR("FAIL: Context.{} error={} > 0.01", fieldName, err);
                  pass = false;
               }
            }
         };

         checkFreq("bandwidth",      original.bandwidth,      f.bandwidth);
         checkFreq("ifRefFrequency", original.ifRefFrequency, f.ifRefFrequency);
         checkFreq("rfFrequency",    original.rfFrequency,    f.rfFrequency);
         checkFreq("rfFreqOffset",   original.rfFreqOffset,   f.rfFreqOffset);
         checkFreq("ifBandOffset",   original.ifBandOffset,   f.ifBandOffset);
         checkFreq("sampleRate",     original.sampleRate,     f.sampleRate);
         checkGain("referenceLevel", original.referenceLevel,  f.referenceLevel);
         checkGain("gain",           original.gain,            f.gain);

         if (f.changeIndicator != original.changeIndicator)
         {
            GPERROR("FAIL: Context.changeIndicator mismatch");
            pass = false;
         }
         if (f.referencePointId != original.referencePointId)
         {
            GPERROR("FAIL: Context.referencePointId mismatch");
            pass = false;
         }
         if (f.overRangeCount != original.overRangeCount)
         {
            GPERROR("FAIL: Context.overRangeCount mismatch");
            pass = false;
         }

         GPINFO("  Context fields:        {}", (pass ? "PASS" : "FAIL"));
         if (!pass) { ++failures; }
      }
   }

   // ----- Summary -----
   GPINFO("==========================================================");
   if (failures == 0)
   {
      GPINFO("ALL TESTS PASSED");
   }
   else
   {
      GPERROR("{} TEST(S) FAILED", failures);
   }
   GPINFO("==========================================================");

   return (failures == 0) ? 0 : 1;
}
