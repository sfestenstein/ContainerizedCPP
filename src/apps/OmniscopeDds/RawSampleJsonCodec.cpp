#include "RawSampleJsonCodec.h"

#include <crow/json.h>

#include <cstdint>
#include <format>

namespace Omniscope
{
namespace RawSampleJsonCodec
{

namespace
{

// Inverse of toJson()'s hex encoder. Returns false on malformed input (odd
// length or non-hex characters) without touching `out`.
bool hexDecode(const std::string &hex, std::vector<uint8_t> &out)
{
   if (hex.size() % 2 != 0) return false;

   auto nibble = [](char c) -> int
   {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
   };

   std::vector<uint8_t> decoded;
   decoded.reserve(hex.size() / 2);
   for (size_t i = 0; i < hex.size(); i += 2)
   {
      int hi = nibble(hex[i]);
      int lo = nibble(hex[i + 1]);
      if (hi < 0 || lo < 0) return false;
      decoded.push_back(static_cast<uint8_t>((hi << 4) | lo));
   }

   out = std::move(decoded);
   return true;
}

} // namespace

std::string toJson(const DdsCore::RawSample &sample)
{
   std::string hex;
   hex.reserve(sample.bytes.size() * 2);
   constexpr char kHex[] = "0123456789abcdef";
   for (uint8_t byte : sample.bytes)
   {
      hex += kHex[byte >> 4];
      hex += kHex[byte & 0xf];
   }

   return std::format(
      R"({{"raw_cdr":"{}","byte_count":{},"type_name":"{}","qos":{{"reliability":"{}","durability":"{}","history_depth":{}}}}})",
      hex, sample.bytes.size(), sample.typeName, sample.reliability, sample.durability,
      sample.historyDepth);
}

std::optional<DdsCore::RawSample> fromJson(const std::string &jsonData)
{
   auto j = crow::json::load(jsonData);
   if (!j) return std::nullopt;

   DdsCore::RawSample sample;
   if (!j.has("raw_cdr") || !hexDecode(j["raw_cdr"].s(), sample.bytes))
      return std::nullopt;

   sample.typeName = j.has("type_name") ? std::string(j["type_name"].s()) : std::string();

   sample.reliability = "reliable";
   sample.durability = "volatile";
   sample.historyDepth = 1;
   if (j.has("qos"))
   {
      auto qos = j["qos"];
      if (qos.has("reliability")) sample.reliability = std::string(qos["reliability"].s());
      if (qos.has("durability")) sample.durability = std::string(qos["durability"].s());
      if (qos.has("history_depth")) sample.historyDepth = static_cast<int32_t>(qos["history_depth"].i());
   }

   return sample;
}

} // namespace RawSampleJsonCodec
} // namespace Omniscope
