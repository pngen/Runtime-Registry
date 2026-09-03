#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[08_capability_query]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  RuntimeInstanceId rt = rr_examples::add_runtime(reg, env, "native", "cpp", "x64");
  ServiceId svc = rr_examples::add_service(reg, env, 200, ServiceKind::MODEL_SERVER, "cuda-svc",
    SemanticVersion::parse("1.0.0").value(), ApiVersion(1,0), {CapabilityKind::CUDA_AVAILABLE});
  CapabilityId cap = rr_examples::add_capability(reg, env, 1, CapabilityKind::CUDA_AVAILABLE, CapabilityValue::make_bool(true));
  EndpointId ep = rr_examples::add_endpoint(reg, env, 1, 31817);
  rr_examples::add_instance(reg, env, svc, env.boot, WorkerId(1), rt, NodeId(1), SemanticVersion::parse("1.0.0").value(), {ep}, {cap});
  RegistryQuery q; q.service_kind = ServiceKind::MODEL_SERVER; q.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  RegistryResult r = reg.query(q);
  rr_examples::print(r);
  std::printf("  explanation: %s\n", reg.explain_query(q).c_str());
  return 0;
}
