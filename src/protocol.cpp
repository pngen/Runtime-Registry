#include <runtimeregistry/protocol.hpp>
#include <runtimeregistry/crc32.hpp>

#include <cstring>

namespace runtimeregistry {

std::string_view to_string(MessageKind k) noexcept {
  switch (k) {
    case MessageKind::HELLO: return "HELLO";
    case MessageKind::REGISTER_RUNTIME: return "REGISTER_RUNTIME";
    case MessageKind::REGISTER_SERVICE: return "REGISTER_SERVICE";
    case MessageKind::REGISTER_ENDPOINT: return "REGISTER_ENDPOINT";
    case MessageKind::REGISTER_CAPABILITY: return "REGISTER_CAPABILITY";
    case MessageKind::UPDATE_HEALTH: return "UPDATE_HEALTH";
    case MessageKind::UPDATE_READINESS: return "UPDATE_READINESS";
    case MessageKind::RENEW_LEASE: return "RENEW_LEASE";
    case MessageKind::INVALIDATE: return "INVALIDATE";
    case MessageKind::TOMBSTONE: return "TOMBSTONE";
    case MessageKind::QUERY: return "QUERY";
    case MessageKind::QUERY_RESULT: return "QUERY_RESULT";
    case MessageKind::SAVE: return "SAVE";
    case MessageKind::RECOVER: return "RECOVER";
    case MessageKind::ACK: return "ACK";
    case MessageKind::ERROR: return "ERROR";
  }
  return "UNKNOWN";
}

MessageKind message_kind_from_u8(std::uint8_t v) noexcept {
  switch (v) {
    case 1: return MessageKind::HELLO;
    case 2: return MessageKind::REGISTER_RUNTIME;
    case 3: return MessageKind::REGISTER_SERVICE;
    case 4: return MessageKind::REGISTER_ENDPOINT;
    case 5: return MessageKind::REGISTER_CAPABILITY;
    case 6: return MessageKind::UPDATE_HEALTH;
    case 7: return MessageKind::UPDATE_READINESS;
    case 8: return MessageKind::RENEW_LEASE;
    case 9: return MessageKind::INVALIDATE;
    case 10: return MessageKind::TOMBSTONE;
    case 11: return MessageKind::QUERY;
    case 12: return MessageKind::QUERY_RESULT;
    case 13: return MessageKind::SAVE;
    case 14: return MessageKind::RECOVER;
    case 15: return MessageKind::ACK;
    case 16: return MessageKind::ERROR;
  }
  return MessageKind::ERROR;
}

namespace {

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

std::uint32_t get_u32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

std::vector<std::uint8_t> encode_frame(const Frame& f) {
  const std::uint32_t len = static_cast<std::uint32_t>(f.payload.size());
  if (len > kMaxPayload) return {};
  std::vector<std::uint8_t> out;
  out.reserve(10 + len);
  put_u32(out, kFrameMagic);
  out.push_back(kFrameVersion);
  out.push_back(static_cast<std::uint8_t>(f.kind));
  put_u32(out, len);
  out.insert(out.end(), f.payload.begin(), f.payload.end());
  // CRC over bytes from index 4 (version) to end of payload.
  std::uint32_t crc = crc32(std::string_view(reinterpret_cast<const char*>(out.data()) + 4, out.size() - 4));
  put_u32(out, crc);
  return out;
}

std::optional<Frame> decode_frame(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr || size < 10) return std::nullopt;
  if (get_u32(data) != kFrameMagic) return std::nullopt;
  if (data[4] != kFrameVersion) return std::nullopt;
  std::uint8_t kind = data[5];
  if (kind < 1 || kind > 16) return std::nullopt;
  std::uint32_t len = get_u32(data + 6);
  if (len > kMaxPayload) return std::nullopt;
  std::size_t content_end = 10 + static_cast<std::size_t>(len);
  std::size_t total = content_end + 4;  // header + payload + CRC
  if (size != total) return std::nullopt;  // truncation or trailing garbage
  std::uint32_t stored_crc = get_u32(data + content_end);
  std::uint32_t computed = crc32(std::string_view(reinterpret_cast<const char*>(data) + 4, content_end - 4));
  if (stored_crc != computed) return std::nullopt;
  Frame f;
  f.kind = message_kind_from_u8(kind);
  f.payload.assign(data + 10, data + 10 + len);
  return f;
}

}  // namespace runtimeregistry
