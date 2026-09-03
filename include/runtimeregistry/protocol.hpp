#pragma once
// Bounded versioned framed TCP protocol used by the coordinator/worker public
// authority surface. Windows loopback sockets. Serializes writes per connection
// and never holds global registry locks during blocking network I/O.

#include <runtimeregistry/identity.hpp>
#include <runtimeregistry/query.hpp>
#include <runtimeregistry/version.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace runtimeregistry {

constexpr std::uint32_t kFrameMagic = 0x52524631u;  // "RRF1"
constexpr std::uint8_t kFrameVersion = 1;
constexpr std::uint32_t kMaxPayload = 64 * 1024;

enum class MessageKind : std::uint8_t {
  HELLO = 1,
  REGISTER_RUNTIME = 2,
  REGISTER_SERVICE = 3,
  REGISTER_ENDPOINT = 4,
  REGISTER_CAPABILITY = 5,
  UPDATE_HEALTH = 6,
  UPDATE_READINESS = 7,
  RENEW_LEASE = 8,
  INVALIDATE = 9,
  TOMBSTONE = 10,
  QUERY = 11,
  QUERY_RESULT = 12,
  SAVE = 13,
  RECOVER = 14,
  ACK = 15,
  ERROR = 16,
};

[[nodiscard]] std::string_view to_string(MessageKind k) noexcept;
[[nodiscard]] MessageKind message_kind_from_u8(std::uint8_t v) noexcept;

struct Frame {
  MessageKind kind{MessageKind::ACK};
  std::vector<std::uint8_t> payload;
};

[[nodiscard]] std::vector<std::uint8_t> encode_frame(const Frame& f);
// Decodes exactly one frame from the head of data. Returns nullopt if more
// bytes are needed or if the buffer is malformed/oversized/truncated.
[[nodiscard]] std::optional<Frame> decode_frame(const std::uint8_t* data,
                                                std::size_t size);

}  // namespace runtimeregistry
