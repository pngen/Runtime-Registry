#pragma once
// CRC-32 (IEEE 802.3) with a deterministic lookup table.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace runtimeregistry {

inline std::uint32_t crc32(std::string_view data, std::uint32_t seed = 0) noexcept {
  // CRC-32 (reflected, polynomial 0xEDB88320), init 0xFFFFFFFF, xor-out 0xFFFFFFFF.
  static std::uint32_t table[256];
  static bool built = false;
  if (!built) {
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      }
      table[i] = c;
    }
    built = true;
  }
  std::uint32_t crc = seed ^ 0xFFFFFFFFu;
  for (unsigned char ch : data) {
    crc = table[(crc ^ ch) & 0xFFu] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu;
}

// Combined CRC of a sequence of string views (used for envelope digests).
inline std::uint32_t crc32_combine(std::initializer_list<std::string_view> parts) noexcept {
  std::uint32_t crc = 0;
  for (std::string_view p : parts) crc = crc32(p, crc);
  return crc;
}

}  // namespace runtimeregistry
