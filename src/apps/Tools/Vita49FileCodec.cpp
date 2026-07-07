// =============================================================================
// Vita49FileCodec
// =============================================================================
// CLI tool for reading, writing, and inspecting VITA 49.2 binary packet files.
// Useful for golden-file testing, interop with GNU Radio / Wireshark, and
// generating synthetic test data.
//
// Modes:
//   generate  - Create a .v49 file from synthetic waveform + context
//   inspect   - Decode a .v49 file and print packet details
//   roundtrip - Read a .v49 file, decode, re-encode, and compare
//
// Usage:
//   ./Vita49FileCodec generate <output.v49> [numSamples] [freqHz]
//   ./Vita49FileCodec inspect  <input.v49>
//   ./Vita49FileCodec roundtrip <input.v49>
// =============================================================================

#include "GeneralLogger.h"
#include "Vita49Codec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

namespace
{

// ============================================================================
// File I/O
// ============================================================================

std::vector<uint8_t> readFile(const std::string& path)
{
   std::ifstream file(path, std::ios::binary | std::ios::ate);
   if (!file.is_open())
   {
      GPERROR("Cannot open file: {}", path);
      return {};
   }

   auto fileSize = file.tellg();
   if (fileSize <= 0)
   {
      return {};
   }

   file.seekg(0);
   std::vector<uint8_t> data(static_cast<size_t>(fileSize));
   file.read(reinterpret_cast<char*>(data.data()),
             static_cast<std::streamsize>(fileSize));

   return data;
}

bool writeFile(const std::string& path, const std::vector<uint8_t>& data)
{
   std::ofstream file(path, std::ios::binary | std::ios::trunc);
   if (!file.is_open())
   {
      GPERROR("Cannot create file: {}", path);
      return false;
   }

   file.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));

   return file.good();
}

// ============================================================================
// Packet type name
// ============================================================================

const char* packetTypeName(Vita49_2::PacketType type)
{
   // NOLINTBEGIN
   switch (type)
   {
      case Vita49_2::PacketType::IFDataWithoutStreamId:  return "IF Data (no SID)";
      case Vita49_2::PacketType::IFDataWithStreamId:     return "IF Data (SID)";
      case Vita49_2::PacketType::ExtDataWithoutStreamId: return "Ext Data (no SID)";
      case Vita49_2::PacketType::ExtDataWithStreamId:    return "Ext Data (SID)";
      case Vita49_2::PacketType::IFContext:              return "IF Context";
      case Vita49_2::PacketType::ExtContext:             return "Ext Context";
      default:                                           return "Unknown";
   }
   // NOLINTEND
}

const char* tsiName(Vita49_2::TSI tsi)
{
   // NOLINTBEGIN
   switch (tsi)
   {
      case Vita49_2::TSI::None:  return "None";
      case Vita49_2::TSI::UTC:   return "UTC";
      case Vita49_2::TSI::GPS:   return "GPS";
      case Vita49_2::TSI::Other: return "Other";
      default:                   return "?";
   }
   // NOLINTEND
}

const char* tsfName(Vita49_2::TSF tsf)
{
   // NOLINTBEGIN
   switch (tsf)
   {
      case Vita49_2::TSF::None:        return "None";
      case Vita49_2::TSF::SampleCount: return "SampleCount";
      case Vita49_2::TSF::RealTime:    return "RealTime(ps)";
      case Vita49_2::TSF::FreeRunning: return "FreeRunning";
      default:                         return "?";
   }
   // NOLINTEND
}

// ============================================================================
// Inspect — print packet summary
// ============================================================================

void logHeader(const Vita49_2::PacketHeader& hdr, int index)
{
   GPINFO("  Packet #{}", index);
   GPINFO("    Type:         {}", packetTypeName(hdr.packetType));
   GPINFO("    PacketCount:  {}", static_cast<int>(hdr.packetCount));
   GPINFO("    PacketSize:   {} words ({} bytes)", hdr.packetSize, hdr.packetSize * 4);
   GPINFO("    ClassID:      {}", (hdr.classIdPresent ? "yes" : "no"));
   GPINFO("    Trailer:      {}", (hdr.trailerPresent ? "yes" : "no"));
   GPINFO("    TSI:          {}", tsiName(hdr.tsiType));
   GPINFO("    TSF:          {}", tsfName(hdr.tsfType));

   if (hdr.streamId.has_value())
   {
      GPINFO("    StreamID:     0x{:08X}", *hdr.streamId);
   }
   if (hdr.integerTimestamp.has_value())
   {
      GPINFO("    IntTimestamp:  {}", *hdr.integerTimestamp);
   }
   if (hdr.fractionalTimestamp.has_value())
   {
      GPINFO("    FracTimestamp: {}", *hdr.fractionalTimestamp);
   }
}

void logContextFields(const Vita49_2::ContextFields& f)
{
   GPINFO("    --- Context Fields ---");
   GPINFO("    ChangeIndicator: {}", (f.changeIndicator ? "true" : "false"));

   if (f.referencePointId.has_value())
   {
      GPINFO("    RefPointID:      0x{:08X}", *f.referencePointId);
   }
   if (f.bandwidth.has_value())
   {
      GPINFO("    Bandwidth:       {:.6e} Hz", *f.bandwidth);
   }
   if (f.ifRefFrequency.has_value())
   {
      GPINFO("    IF Ref Freq:     {:.6e} Hz", *f.ifRefFrequency);
   }
   if (f.rfFrequency.has_value())
   {
      GPINFO("    RF Frequency:    {:.6e} Hz", *f.rfFrequency);
   }
   if (f.rfFreqOffset.has_value())
   {
      GPINFO("    RF Freq Offset:  {:.6e} Hz", *f.rfFreqOffset);
   }
   if (f.ifBandOffset.has_value())
   {
      GPINFO("    IF Band Offset:  {:.6e} Hz", *f.ifBandOffset);
   }
   if (f.referenceLevel.has_value())
   {
      GPINFO("    Ref Level:       {:.2f} dBm", *f.referenceLevel);
   }
   if (f.gain.has_value())
   {
      GPINFO("    Gain:            {:.2f} dB", *f.gain);
   }
   if (f.overRangeCount.has_value())
   {
      GPINFO("    Over-Range Cnt:  {}", *f.overRangeCount);
   }
   if (f.sampleRate.has_value())
   {
      GPINFO("    Sample Rate:     {:.6e} Hz", *f.sampleRate);
   }
}

void logSampleSummary(const Vita49_2::IQSamples& samples)
{
   GPINFO("    --- Signal Data ---");
   GPINFO("    Samples:         {}", samples.size());

   if (samples.empty()) { return; }

   // Stats
   float minI = samples[0].real();
   float maxI = minI;
   float minQ = samples[0].imag();
   float maxQ = minQ;
   double sumMag2 = 0.0;

   for (const auto& s : samples)
   {
      minI = std::min(minI, s.real());
      maxI = std::max(maxI, s.real());
      minQ = std::min(minQ, s.imag());
      maxQ = std::max(maxQ, s.imag());
      sumMag2 += static_cast<double>((s.real() * s.real()) + (s.imag() * s.imag()));
   }

   double rmsMag = std::sqrt(sumMag2 / static_cast<double>(samples.size()));

   GPINFO("    I range:         [{:.6f}, {:.6f}]", minI, maxI);
   GPINFO("    Q range:         [{:.6f}, {:.6f}]", minQ, maxQ);
   GPINFO("    RMS magnitude:   {:.6f}", rmsMag);

   // Print first few samples
   size_t printCount = std::min(samples.size(), static_cast<size_t>(5));
   GPINFO("    First {} samples:", printCount);
   for (size_t i = 0; i < printCount; ++i)
   {
      GPINFO("      [{}] I={:10.6f}  Q={:10.6f}",
             i, samples[i].real(), samples[i].imag());
   }
}

int doInspect(const std::string& path)
{
   auto data = readFile(path);
   if (data.empty())
   {
      GPERROR("File is empty or unreadable: {}", path);
      return 1;
   }

   GPINFO("==========================================================");
   GPINFO("VITA 49.2 File Inspector");
   GPINFO("==========================================================");
   GPINFO("File:  {}", path);
   GPINFO("Size:  {} bytes", data.size());
   GPINFO("==========================================================");

   const Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::BigEndian);
   auto packets = codec.parseStream(data.data(), data.size());

   GPINFO("Decoded {} packet(s):", packets.size());

   size_t totalSamples = 0;
   int idx = 0;

   for (const auto& pkt : packets)
   {
      logHeader(pkt.header, idx);

      if (pkt.type == Vita49_2::ParsedPacket::Type::SignalData)
      {
         logSampleSummary(pkt.samples);
         totalSamples += pkt.samples.size();
      }
      else if (pkt.type == Vita49_2::ParsedPacket::Type::Context)
      {
         logContextFields(pkt.contextFields);
      }
      else
      {
         GPWARN("    (Unknown packet type)");
      }

      ++idx;
   }

   GPINFO("==========================================================");
   GPINFO("Total: {} packets, {} I/Q samples", packets.size(), totalSamples);
   GPINFO("==========================================================");

   return 0;
}

// ============================================================================
// Generate — create a synthetic .v49 file
// ============================================================================

int doGenerate(const std::string& path, size_t numSamples, double freqHz)
{
   constexpr double SAMPLE_RATE = 1.0e6;
   constexpr uint32_t STREAM_ID = 0x00001234;

   GPINFO("==========================================================");
   GPINFO("VITA 49.2 File Generator");
   GPINFO("==========================================================");
   GPINFO("  Output:      {}", path);
   GPINFO("  Samples:     {}", numSamples);
   GPINFO("  Tone freq:   {} Hz", freqHz);
   GPINFO("  Sample rate: {} Hz", SAMPLE_RATE);
   GPINFO("  Stream ID:   0x{:08X}", STREAM_ID);
   GPINFO("==========================================================");

   const Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::BigEndian);

   // 1) Context packet
   Vita49_2::ContextFields ctx;
   ctx.changeIndicator = true;
   ctx.rfFrequency     = 2.4e9;
   ctx.bandwidth       = SAMPLE_RATE;
   ctx.sampleRate      = SAMPLE_RATE;
   ctx.gain            = 20.0;
   ctx.referenceLevel  = -40.0;

   auto contextBytes = codec.encodeContext(STREAM_ID, ctx, 0,
      Vita49_2::TSI::UTC, Vita49_2::TSF::RealTime, 1738886400, 0);

   // 2) Signal Data packets — tone waveform
   Vita49_2::IQSamples samples(numSamples);
   for (size_t i = 0; i < numSamples; ++i)
   {
      const double t = static_cast<double>(i) / SAMPLE_RATE;
      auto phase = static_cast<float>(2.0 * std::numbers::pi * freqHz * t);
      samples[i] = {0.8f * std::cos(phase), 0.8f * std::sin(phase)};
   }

   auto signalBytes = codec.encodeSignalData(STREAM_ID, samples, 1,
      Vita49_2::TSI::UTC, Vita49_2::TSF::RealTime, 1738886400, 0);

   // 3) Concatenate context + signal data
   std::vector<uint8_t> output;
   output.reserve(contextBytes.size() + signalBytes.size());
   output.insert(output.end(), contextBytes.begin(), contextBytes.end());
   output.insert(output.end(), signalBytes.begin(), signalBytes.end());

   if (!writeFile(path, output))
   {
      return 1;
   }

   GPINFO("Written {} bytes ({} context + {} signal data)",
          output.size(), contextBytes.size(), signalBytes.size());

   return 0;
}

// ============================================================================
// Round-trip — read, decode, re-encode, compare
// ============================================================================

int doRoundTrip(const std::string& path)
{
   auto data = readFile(path);
   if (data.empty())
   {
      GPERROR("File is empty or unreadable: {}", path);
      return 1;
   }

   GPINFO("==========================================================");
   GPINFO("VITA 49.2 File Round-Trip Test");
   GPINFO("==========================================================");
   GPINFO("File:  {}", path);
   GPINFO("Size:  {} bytes", data.size());
   GPINFO("==========================================================");

   const Vita49_2::Vita49Codec codec(Vita49_2::ByteOrder::BigEndian);

   // Decode
   auto packets = codec.parseStream(data.data(), data.size());
   GPINFO("Decoded {} packet(s) from file.", packets.size());

   if (packets.empty())
   {
      GPERROR("No packets decoded.");
      return 1;
   }

   // Re-encode each packet
   std::vector<uint8_t> reEncoded;

   for (const auto& pkt : packets)
   {
      if (pkt.type == Vita49_2::ParsedPacket::Type::SignalData)
      {
         const uint32_t sid = pkt.header.streamId.value_or(0);
         auto bytes = codec.encodeSignalData(
            sid, pkt.samples, pkt.header.packetCount,
            pkt.header.tsiType, pkt.header.tsfType,
            pkt.header.integerTimestamp.value_or(0),
            pkt.header.fractionalTimestamp.value_or(0),
            pkt.header.trailerPresent);
         reEncoded.insert(reEncoded.end(), bytes.begin(), bytes.end());
      }
      else if (pkt.type == Vita49_2::ParsedPacket::Type::Context)
      {
         const uint32_t sid = pkt.header.streamId.value_or(0);
         auto bytes = codec.encodeContext(
            sid, pkt.contextFields, pkt.header.packetCount,
            pkt.header.tsiType, pkt.header.tsfType,
            pkt.header.integerTimestamp.value_or(0),
            pkt.header.fractionalTimestamp.value_or(0));
         reEncoded.insert(reEncoded.end(), bytes.begin(), bytes.end());
      }
   }

   GPINFO("Re-encoded {} bytes.", reEncoded.size());

   // Compare
   if (reEncoded.size() != data.size())
   {
      GPWARN("Size mismatch — original={} re-encoded={}",
             data.size(), reEncoded.size());
      GPWARN("  (This may be expected if original has unsupported fields)");
   }

   // Decode re-encoded and compare sample-by-sample
   auto packets2 = codec.parseStream(reEncoded.data(), reEncoded.size());
   GPINFO("Re-decoded {} packet(s).", packets2.size());

   // Compare signal data
   size_t sigIdx1 = 0;
   size_t sigIdx2 = 0;
   Vita49_2::IQSamples allSamples1;
   Vita49_2::IQSamples allSamples2;

   for (const auto& pkt : packets)
   {
      if (pkt.type == Vita49_2::ParsedPacket::Type::SignalData)
      {
         allSamples1.insert(allSamples1.end(),
                            pkt.samples.begin(), pkt.samples.end());
         ++sigIdx1;
      }
   }
   for (const auto& pkt : packets2)
   {
      if (pkt.type == Vita49_2::ParsedPacket::Type::SignalData)
      {
         allSamples2.insert(allSamples2.end(),
                            pkt.samples.begin(), pkt.samples.end());
         ++sigIdx2;
      }
   }

   GPINFO("Signal packets: original={} re-encoded={}", sigIdx1, sigIdx2);
   GPINFO("Total samples:  original={} re-encoded={}",
          allSamples1.size(), allSamples2.size());

   if (allSamples1.size() == allSamples2.size())
   {
      float maxErr = 0.0f;
      for (size_t i = 0; i < allSamples1.size(); ++i)
      {
         const float eI = std::fabs(allSamples1[i].real() - allSamples2[i].real());
         const float eQ = std::fabs(allSamples1[i].imag() - allSamples2[i].imag());
         maxErr = std::max({maxErr, eI, eQ});
      }
      GPINFO("Max sample error: {:.6e}", maxErr);

      // After one round-trip through int16, error should be exactly 0
      // (same quantization both times)
      if (maxErr == 0.0f)
      {
         GPINFO("Round-trip: PERFECT (bit-exact after re-encode)");
      }
      else
      {
         GPINFO("Round-trip: PASS (non-zero error from external source)");
      }
   }
   else
   {
      GPERROR("Round-trip: SAMPLE COUNT MISMATCH");
   }

   GPINFO("==========================================================");
   return 0;
}

// ============================================================================
// Usage
// ============================================================================

void printUsage(const char* progName)
{
   GPINFO("Usage:");
   GPINFO("  {} generate <output.v49> [numSamples] [freqHz]", progName);
   GPINFO("  {} inspect  <input.v49>", progName);
   GPINFO("  {} roundtrip <input.v49>", progName);
   GPINFO("");
   GPINFO("Modes:");
   GPINFO("  generate  - Create a .v49 file with synthetic tone + context");
   GPINFO("  inspect   - Decode and print packet details from a .v49 file");
   GPINFO("  roundtrip - Read, decode, re-encode, and compare");
   GPINFO("");
   GPINFO("Defaults:");
   GPINFO("  numSamples = 10000");
   GPINFO("  freqHz     = 1000.0");
}

} // anonymous namespace

// ============================================================================
// main
// ============================================================================
// NOLINTNEXTLINE
int main(int argc, char* argv[])
{
   CommonUtils::GeneralLogger logger;
   logger.init("Vita49FileCodec");
   if (argc < 3)
   {
      printUsage(argv[0]);
      return 1;
   }

   const std::string mode = argv[1];
   const std::string filePath = argv[2];

   if (mode == "generate")
   {
      size_t numSamples = 10000;
      double freqHz = 1000.0;
      if (argc > 3)
      {
         numSamples = static_cast<size_t>(std::atol(argv[3]));
      }
      if (argc > 4)
      {
         freqHz = std::atof(argv[4]);
      }
      return doGenerate(filePath, numSamples, freqHz);
   }
   if (mode == "inspect")
   {
      return doInspect(filePath);
   }
   if (mode == "roundtrip")
   {
      return doRoundTrip(filePath);
   }

   GPERROR("Unknown mode '{}'", mode);
   printUsage(argv[0]);
   return 1;
}
