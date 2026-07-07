#ifndef BYTESWAP_H_
#define BYTESWAP_H_

#include <cstdint>

namespace Vita49_2
{

// ============================================================================
// Big-Endian Read
// ============================================================================

inline uint16_t readU16BE(const uint8_t* p)
{
   return static_cast<uint16_t>(
      (static_cast<uint16_t>(p[0]) << 8) |
       static_cast<uint16_t>(p[1]));
}

inline uint32_t readU32BE(const uint8_t* p)
{
   return (static_cast<uint32_t>(p[0]) << 24) |
          (static_cast<uint32_t>(p[1]) << 16) |
          (static_cast<uint32_t>(p[2]) << 8)  |
           static_cast<uint32_t>(p[3]);
}

inline uint64_t readU64BE(const uint8_t* p)
{
   return (static_cast<uint64_t>(readU32BE(p)) << 32) |
           static_cast<uint64_t>(readU32BE(p + 4));
}

inline int16_t readI16BE(const uint8_t* p)
{
   return static_cast<int16_t>(readU16BE(p));
}

// ============================================================================
// Big-Endian Write
// ============================================================================

inline void writeU16BE(uint8_t* p, uint16_t val)
{
   p[0] = static_cast<uint8_t>((val >> 8) & 0xFFU);
   p[1] = static_cast<uint8_t>(val & 0xFFU);
}

inline void writeU32BE(uint8_t* p, uint32_t val)
{
   p[0] = static_cast<uint8_t>((val >> 24) & 0xFFU);
   p[1] = static_cast<uint8_t>((val >> 16) & 0xFFU);
   p[2] = static_cast<uint8_t>((val >> 8) & 0xFFU);
   p[3] = static_cast<uint8_t>(val & 0xFFU);
}

inline void writeU64BE(uint8_t* p, uint64_t val)
{
   writeU32BE(p, static_cast<uint32_t>((val >> 32) & 0xFFFFFFFFU));
   writeU32BE(p + 4, static_cast<uint32_t>(val & 0xFFFFFFFFU));
}

inline void writeI16BE(uint8_t* p, int16_t val)
{
   writeU16BE(p, static_cast<uint16_t>(val));
}

// ============================================================================
// Little-Endian Read
// ============================================================================

inline uint16_t readU16LE(const uint8_t* p)
{
   return static_cast<uint16_t>(
       static_cast<uint16_t>(p[0]) |
      (static_cast<uint16_t>(p[1]) << 8));
}

inline uint32_t readU32LE(const uint8_t* p)
{
   return  static_cast<uint32_t>(p[0])        |
          (static_cast<uint32_t>(p[1]) << 8)  |
          (static_cast<uint32_t>(p[2]) << 16) |
          (static_cast<uint32_t>(p[3]) << 24);
}

inline uint64_t readU64LE(const uint8_t* p)
{
   return  static_cast<uint64_t>(readU32LE(p)) |
          (static_cast<uint64_t>(readU32LE(p + 4)) << 32);
}

inline int16_t readI16LE(const uint8_t* p)
{
   return static_cast<int16_t>(readU16LE(p));
}

// ============================================================================
// Little-Endian Write
// ============================================================================

inline void writeU16LE(uint8_t* p, uint16_t val)
{
   p[0] = static_cast<uint8_t>(val & 0xFFU);
   p[1] = static_cast<uint8_t>((val >> 8) & 0xFFU);
}

inline void writeU32LE(uint8_t* p, uint32_t val)
{
   p[0] = static_cast<uint8_t>(val & 0xFFU);
   p[1] = static_cast<uint8_t>((val >> 8) & 0xFFU);
   p[2] = static_cast<uint8_t>((val >> 16) & 0xFFU);
   p[3] = static_cast<uint8_t>((val >> 24) & 0xFFU);
}

inline void writeU64LE(uint8_t* p, uint64_t val)
{
   writeU32LE(p, static_cast<uint32_t>(val & 0xFFFFFFFFU));
   writeU32LE(p + 4, static_cast<uint32_t>((val >> 32) & 0xFFFFFFFFU));
}

inline void writeI16LE(uint8_t* p, int16_t val)
{
   writeU16LE(p, static_cast<uint16_t>(val));
}

} // namespace Vita49_2

#endif // BYTESWAP_H_
