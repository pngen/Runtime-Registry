#include <runtimeregistry/protocol.hpp>
#include <runtimeregistry/crc32.hpp>
#include "test_util.hpp"
using namespace runtimeregistry;

namespace {
void test_frame_round_trip() {
  Frame f;
  f.kind = MessageKind::REGISTER_RUNTIME;
  std::vector<std::uint8_t> payload = {1,2,3,4,5};
  f.payload = payload;
  std::vector<std::uint8_t> enc = encode_frame(f);
  CHECK(!enc.empty());
  auto dec = decode_frame(enc.data(), enc.size());
  CHECK(dec.has_value());
  CHECK(dec->kind == MessageKind::REGISTER_RUNTIME);
  CHECK(dec->payload == payload);
}

void test_frame_rejections() {
  Frame f; f.kind = MessageKind::ACK; f.payload = {0xAA,0xBB};
  std::vector<std::uint8_t> enc = encode_frame(f);
  CHECK(!enc.empty());
  // bad magic
  std::vector<std::uint8_t> bad = enc; bad[0] ^= 0xFF;
  CHECK(!decode_frame(bad.data(), bad.size()).has_value());
  // unsupported version
  std::vector<std::uint8_t> v = enc; v[4] = 99;
  CHECK(!decode_frame(v.data(), v.size()).has_value());
  // invalid enum kind
  std::vector<std::uint8_t> k = enc; k[5] = 200;
  CHECK(!decode_frame(k.data(), k.size()).has_value());
  // truncation
  CHECK(!decode_frame(enc.data(), enc.size()-1).has_value());
  // checksum mismatch
  std::vector<std::uint8_t> c = enc; c[c.size()-5] ^= 0x01;
  CHECK(!decode_frame(c.data(), c.size()).has_value());
  // trailing garbage
  std::vector<std::uint8_t> t = enc; t.push_back(0xFF);
  CHECK(!decode_frame(t.data(), t.size()).has_value());
  // oversized payload (parse a crafted length)
  std::vector<std::uint8_t> big = enc; big[6] = 0xFF; big[7] = 0xFF; big[8] = 0xFF; big[9] = 0xFF;
  CHECK(!decode_frame(big.data(), big.size()).has_value());
  // too short
  CHECK(!decode_frame(enc.data(), 3).has_value());
}
}  // namespace

void test_protocol_suite();
void test_protocol_suite() { test_frame_round_trip(); test_frame_rejections(); }
RR_REGISTER(test_protocol_suite);
int main() { return rr_test::run_all("protocol"); }
