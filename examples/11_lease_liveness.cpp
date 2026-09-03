#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[11_lease_liveness]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  LeaseDescriptor l; l.boot = env.boot; l.state = LeaseState::ACTIVE; l.renewal_policy = LeaseRenewalPolicy::BOOT;
  l.provenance = make_provenance(EvidenceKind::REPORTED, "worker", 0);
  LeaseId id = reg.acquire_lease(l, env);
  std::printf("  acquired lease=%llu state=%s\n", (unsigned long long)id.value(), to_string(reg.find_lease(id)->state).data());
  env.lease_generation = reg.find_lease(id)->generation;
  reg.renew_lease(id, env);
  std::printf("  renewed state=%s\n", to_string(reg.find_lease(id)->state).data());
  reg.expire_lease(id, env);
  std::printf("  expired state=%s\n", to_string(reg.find_lease(id)->state).data());
  return 0;
}
