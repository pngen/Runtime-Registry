#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[13_persistence_recovery]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  RuntimeInstanceId rt = rr_examples::add_runtime(reg, env, "native", "cpp", "x64");
  ServiceId svc = rr_examples::add_service(reg, env, 600, ServiceKind::REGISTRY_SERVICE, "reg", SemanticVersion::parse("1.0.0").value(), ApiVersion(1,0));
  EndpointId ep = rr_examples::add_endpoint(reg, env, 1, 31817);
  rr_examples::add_instance(reg, env, svc, env.boot, WorkerId(1), rt, NodeId(1), SemanticVersion::parse("1.0.0").value(), {ep}, {});
  std::vector<std::uint8_t> bytes = reg.serialize();
  std::string d1 = reg.semantic_digest();
  std::printf("  serialized=%zu bytes digest=%s\n", bytes.size(), d1.c_str());
  Registry r2; r2.load_from(bytes);
  std::vector<std::uint8_t> b2 = r2.serialize();
  Registry r3; r3.load_from(b2);
  std::vector<std::uint8_t> b3 = r3.serialize();
  std::printf("  recovery_idempotent=%d service_preserved=%d\n", (int)(b2==b3), (int)(r2.find_service(svc)!=nullptr));
  std::printf("  recovered_live_boot_cleared=%d\n", (int)(!r2.authority().is_boot_live(env.boot)));
  return 0;
}
