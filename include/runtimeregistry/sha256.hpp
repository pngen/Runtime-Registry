#pragma once
// SHA-256 (FIPS 180-4). Deterministic; used for persistence semantic digests.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace runtimeregistry {

using Sha256 = std::array<std::uint8_t, 32>;

[[nodiscard]] Sha256 sha256(std::string_view data) noexcept;

[[nodiscard]] std::string sha256_hex(std::string_view data);

[[nodiscard]] std::string sha256_hex_of(const Sha256& digest);

}  // namespace runtimeregistry
