#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[05_exact_discovery]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  RuntimeInstanceId rt = rr_examples::add_runtime(reg, env, "native", "cpp", "x64");
  ServiceId svc = rr_examples::add_service(reg, env, 100, ServiceKind::MODEL_SERVER, "inference",
    SemanticVersion::parse("2.0.0").value(), ApiVersion(2,0));
  CapabilityId cap = rr_examples::add_capability(reg, env, 1, CapabilityKind::CUDA_AVAILABLE, CapabilityValue::make_bool(true));
  EndpointId ep = rr_examples::add_endpoint(reg, env, 1, 31817);
  ServiceInstanceId inst = rr_examples::add_instance(reg, env, svc, env.boot, WorkerId(1), rt, NodeId(1),
    SemanticVersion::parse("2.0.0").value(), {ep}, {cap});
  RegistryQuery q; q.exact_service_id = svc;
  RegistryResult r = reg.query_and_account(q);
  rr_examples::print(r);
  std::printf("  selected_count=%zu\n", r.ranked.size());
  return 0;
}
