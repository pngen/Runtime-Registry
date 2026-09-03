#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[06_version_filter]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  RuntimeInstanceId rt = rr_examples::add_runtime(reg, env, "native", "cpp", "x64");
  ServiceId s1 = rr_examples::add_service(reg, env, 101, ServiceKind::MODEL_SERVER, "old", SemanticVersion::parse("1.0.0").value(), ApiVersion(1,0));
  ServiceId s2 = rr_examples::add_service(reg, env, 102, ServiceKind::MODEL_SERVER, "new", SemanticVersion::parse("2.0.0").value(), ApiVersion(2,0));
  CapabilityId cap = rr_examples::add_capability(reg, env, 1, CapabilityKind::CUDA_AVAILABLE, CapabilityValue::make_bool(true));
  EndpointId e1 = rr_examples::add_endpoint(reg, env, 1, 31817); EndpointId e2 = rr_examples::add_endpoint(reg, env, 2, 31818);
  rr_examples::add_instance(reg, env, s1, env.boot, WorkerId(1), rt, NodeId(1), SemanticVersion::parse("1.0.0").value(), {e1}, {cap});
  rr_examples::add_instance(reg, env, s2, env.boot, WorkerId(1), rt, NodeId(1), SemanticVersion::parse("2.0.0").value(), {e2}, {cap});
  RegistryQuery q; q.service_kind = ServiceKind::MODEL_SERVER; q.has_minimum_api = true; q.minimum_api = ApiVersion(2,0);
  RegistryResult r = reg.query(q);
  rr_examples::print(r);
  std::printf("  done\n");
  return 0;
}
