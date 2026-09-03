#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[10_reachability]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  RuntimeInstanceId rt = rr_examples::add_runtime(reg, env, "native", "cpp", "x64");
  ServiceId svc = rr_examples::add_service(reg, env, 400, ServiceKind::TRANSFER_SERVICE, "transfer", SemanticVersion::parse("1.0.0").value(), ApiVersion(1,0));
  EndpointId ep = rr_examples::add_endpoint(reg, env, 1, 31817);
  ServiceInstanceId inst = rr_examples::add_instance(reg, env, svc, env.boot, WorkerId(1), rt, NodeId(1), SemanticVersion::parse("1.0.0").value(), {ep}, {});
  std::printf("  endpoint reachability=%s\n", to_string(reg.find_endpoint(ep)->reachability).data());
  reg.update_reachability(ep, Reachability::UNREACHABLE, env);
  std::printf("  after unreachable, instance lifecycle=%s reach=%s\n", to_string(reg.find_instance(inst)->lifecycle).data(), to_string(reg.find_instance(inst)->reachability).data());
  return 0;
}
