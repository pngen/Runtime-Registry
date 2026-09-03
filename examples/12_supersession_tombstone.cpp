#include "example_util.hpp"
using namespace runtimeregistry;
int main() {
  std::printf("[12_supersession_tombstone]\n");
  Registry reg; AuthorityEnvelope env = rr_examples::setup(reg);
  RuntimeInstanceId rt = rr_examples::add_runtime(reg, env, "native", "cpp", "x64");
  ServiceId svc = rr_examples::add_service(reg, env, 500, ServiceKind::MODEL_SERVER, "svc", SemanticVersion::parse("1.0.0").value(), ApiVersion(1,0));
  EndpointId ep = rr_examples::add_endpoint(reg, env, 1, 31817);
  ServiceInstanceId old_inst = rr_examples::add_instance(reg, env, svc, env.boot, WorkerId(1), rt, NodeId(1), SemanticVersion::parse("1.0.0").value(), {ep}, {}, Health::HEALTHY, Readiness::READY, 1);
  env.service_instance_generation = ServiceInstanceGeneration(2);
  ServiceInstanceId new_inst = rr_examples::add_instance(reg, env, svc, env.boot, WorkerId(1), rt, NodeId(1), SemanticVersion::parse("2.0.0").value(), {ep}, {}, Health::HEALTHY, Readiness::READY, 2);
  std::printf("  old_lifecycle=%s new_lifecycle=%s\n", to_string(reg.find_instance(old_inst)->lifecycle).data(), to_string(reg.find_instance(new_inst)->lifecycle).data());
  TombstoneRecord tomb; tomb.target_kind="ServiceId"; tomb.target_text=std::to_string(svc.value()); tomb.generation_floor=RecordGeneration(9);
  tomb.epoch=env.epoch; tomb.boot=env.boot; tomb.reason="retired";
  reg.create_tombstone(tomb, env);
  ServiceInstance x; x.service_id=svc; x.service_generation=ServiceGeneration(1); x.instance_generation=ServiceInstanceGeneration(1);
  x.node=NodeId(1); x.worker=WorkerId(1); x.boot=env.boot; x.runtime_id=RuntimeId(1); x.runtime_instance=rt;
  x.version=SemanticVersion::parse("1.0.0").value(); x.lifecycle=Lifecycle::REGISTERING;
  bool rejected=false; try { reg.register_instance(x, env); } catch (const RegistryError& e) { rejected = (e.kind()==ErrorKind::TOMBSTONE_RESURRECTION); }
  std::printf("  tombstone_resurrection_rejected=%d\n", (int)rejected);
  return 0;
}
