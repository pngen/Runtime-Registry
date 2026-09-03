#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[02_runtime_registration]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  rr_examples::add_runtime(reg, env, "native-cpp", "cpp", "x64");
  const RuntimeDescriptor* rt = reg.find_runtime(RuntimeId(1));
  std::printf("  runtime=%s family=%s arch=%s\n", rt ? rt->name.c_str() : "?", rt ? rt->family.c_str() : "?", rt ? rt->architecture.c_str() : "?");
  std::printf("  invariants=%zu\n", reg.check_invariants().size());
  return 0;
}
