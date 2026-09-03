#include "example_util.hpp"
#include <runtimeregistry/selection.hpp>
using namespace runtimeregistry;
int main() {
  std::printf("[07_protocol_negotiation]\n");
  ProtocolVersion req(2,0), offered(2,1), old(1,9), exact(2,0), mismatch(3,0);
  std::printf("  2.0 vs 2.0 -> %s\n", to_string(negotiate_protocol(req, exact, false)).data());
  std::printf("  2.0 vs 2.1 -> %s\n", to_string(negotiate_protocol(req, offered, false)).data());
  std::printf("  2.1 vs 2.0 -> %s\n", to_string(negotiate_protocol(ProtocolVersion(2,1), req, false)).data());
  std::printf("  2.0 vs 1.9 -> %s\n", to_string(negotiate_protocol(req, old, false)).data());
  std::printf("  2.0 vs 3.0 -> %s\n", to_string(negotiate_protocol(req, mismatch, false)).data());
  std::printf("  2.0 vs 2.1 (exact) -> %s\n", to_string(negotiate_protocol(req, offered, true)).data());
  return 0;
}
