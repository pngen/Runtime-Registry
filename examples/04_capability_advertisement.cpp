#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[04_capability_advertisement]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  CapabilityId c = rr_examples::add_capability(reg, env, 1, CapabilityKind::CUDA_AVAILABLE, CapabilityValue::make_bool(true));
  CapabilityId cc = rr_examples::add_capability(reg, env, 2, CapabilityKind::CUDA_COMPUTE_CAPABILITY, CapabilityValue::make_string("12.0"));
  const CapabilityDescriptor* k = reg.find_capability(cc);
  std::printf("  capability=%s value=%s freshness=%s\n", k ? to_string(k->kind).data() : "?",
              k ? k->value.render().c_str() : "?", k ? to_string(k->freshness).data() : "?");
  return 0;
}
