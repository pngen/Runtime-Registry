#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[09_health_readiness]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  RuntimeInstanceId rt = rr_examples::add_runtime(reg, env, "native", "cpp", "x64");
  ServiceId svc = rr_examples::add_service(reg, env, 300, ServiceKind::HEALTH_AGENT, "health", SemanticVersion::parse("1.0.0").value(), ApiVersion(1,0));
  EndpointId ep = rr_examples::add_endpoint(reg, env, 1, 31817);
  ServiceInstanceId inst = rr_examples::add_instance(reg, env, svc, env.boot, WorkerId(1), rt, NodeId(1), SemanticVersion::parse("1.0.0").value(), {ep}, {});
  std::printf("  initial lifecycle=%s\n", to_string(reg.find_instance(inst)->lifecycle).data());
  env.service_instance_generation = ServiceInstanceGeneration(1);
  reg.update_readiness(inst, Readiness::NOT_READY, env);
  std::printf("  after readiness=NOT_READY lifecycle=%s\n", to_string(reg.find_instance(inst)->lifecycle).data());
  reg.update_health(inst, Health::DEGRADED, env);
  std::printf("  after health=DEGRADED lifecycle=%s\n", to_string(reg.find_instance(inst)->lifecycle).data());
  reg.update_readiness(inst, Readiness::READY, env); reg.update_health(inst, Health::HEALTHY, env);
  std::printf("  restored lifecycle=%s\n", to_string(reg.find_instance(inst)->lifecycle).data());
  return 0;
}
