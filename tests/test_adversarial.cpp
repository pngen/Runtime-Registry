#include <runtimeregistry/registry.hpp>
#include "test_util.hpp"
using namespace runtimeregistry;

namespace {
ServiceInstance dup_inst(ServiceId, RuntimeId, RuntimeInstanceId, int, WorkerBootId);

struct A {
  Registry reg; WorkerBootId boot; AuthorityEnvelope env; ServiceId service; RuntimeId runtime; RuntimeInstanceId rtinst; ServiceInstanceId inst;
  A() { reg.begin_coordinator_epoch(); boot = reg.adopt_worker(WorkerId(1)); env.epoch=reg.authority().epoch; env.boot=boot;
    RuntimeDescriptor rt; rt.runtime_id=RuntimeId(1); rt.runtime_instance=RuntimeInstanceId(1); rt.generation=RuntimeGeneration(1);
    rt.kind=RuntimeKind::NATIVE_CPP; rt.name="r"; rt.family="cpp"; rt.version=SemanticVersion::parse("1.0.0").value();
    rt.api_version=ApiVersion(1,0); rt.abi_version=AbiVersion(1,0); reg.register_runtime(rt, env); runtime=rt.runtime_id; rtinst=rt.runtime_instance;
    ServiceDescriptor svc; svc.service_id=ServiceId(200); svc.kind=ServiceKind::SCHEDULER; svc.name="s";
    svc.generation=ServiceGeneration(5); svc.version=SemanticVersion::parse("1.0.0").value();
    svc.api_version=ApiVersion(1,0); svc.abi_version=AbiVersion(1,0); reg.register_service(svc, env); service=svc.service_id;
    EndpointDescriptor ep; ep.endpoint_id=EndpointId(1); ep.generation=EndpointGeneration(1); ep.protocol=ProtocolId(1);
    ep.locator.text="127.0.0.1:4000"; ep.transport=TransportKind::TCP; ep.health=Health::HEALTHY;
    ep.reachability=Reachability::REACHABLE; ep.freshness=Freshness::CURRENT; ep.provenance=make_provenance(EvidenceKind::MEASURED,"p",0);
    reg.register_endpoint(ep, env);
    ServiceInstance i; i.service_id=service; i.service_generation=ServiceGeneration(5); i.instance_generation=ServiceInstanceGeneration(10);
    i.node=NodeId(1); i.worker=WorkerId(1); i.boot=boot; i.runtime_id=runtime; i.runtime_instance=rtinst; i.endpoints={EndpointId(1)};
    i.protocols={ProtocolId(1)}; i.health=Health::HEALTHY; i.readiness=Readiness::READY; i.freshness=Freshness::CURRENT;
    i.reachability=Reachability::REACHABLE; i.version=SemanticVersion::parse("1.0.0").value();
    i.provenance=make_provenance(EvidenceKind::REPORTED,"w",0); i.lifecycle=Lifecycle::REGISTERING;
    inst = reg.register_instance(i, env);
    env.service_instance_generation = ServiceInstanceGeneration(10);
  }
};

void test_stale_epoch() {
  A a; AuthorityEnvelope bad = a.env; bad.epoch = CoordinatorEpoch(1);
  CHECK_THROWS(a.reg.update_health(a.inst, Health::HEALTHY, bad), RegistryError, ErrorKind::STALE_EPOCH);
}
void test_stale_boot() {
  A a; AuthorityEnvelope bad = a.env; bad.boot = WorkerBootId(99999);
  CHECK_THROWS(a.reg.update_health(a.inst, Health::HEALTHY, bad), RegistryError, ErrorKind::STALE_BOOT);
}
void test_stale_health_update() {
  A a; AuthorityEnvelope bad = a.env; bad.service_instance_generation = ServiceInstanceGeneration(9);
  CHECK_THROWS(a.reg.update_health(a.inst, Health::HEALTHY, bad), RegistryError, ErrorKind::STALE_HEALTH);
}
void test_stale_readiness_update() {
  A a; AuthorityEnvelope bad = a.env; bad.service_instance_generation = ServiceInstanceGeneration(1);
  CHECK_THROWS(a.reg.update_readiness(a.inst, Readiness::READY, bad), RegistryError, ErrorKind::STALE_READINESS);
}
void test_stale_service_generation() {
  A a; ServiceDescriptor svc; svc.service_id = a.service; svc.kind = ServiceKind::SCHEDULER; svc.name="s";
  svc.generation = ServiceGeneration(4); svc.version=SemanticVersion::parse("1.0.0").value();
  svc.api_version=ApiVersion(1,0); svc.abi_version=AbiVersion(1,0);
  CHECK_THROWS(a.reg.register_service(svc, a.env), RegistryError, ErrorKind::STALE_SERVICE_GENERATION);
}
void test_conflicting_duplicate_instance() {
  A a; ServiceInstance dup; dup.service_id=a.service; dup.service_generation=ServiceGeneration(5);
  dup.instance_generation=ServiceInstanceGeneration(10); dup.node=NodeId(1); dup.worker=WorkerId(1); dup.boot=a.boot;
  dup.runtime_id=a.runtime; dup.runtime_instance=a.rtinst; dup.version=SemanticVersion::parse("1.0.0").value();
  dup.lifecycle=Lifecycle::REGISTERING;
  // same generation as current but different content -> conflicting duplicate
  CHECK_THROWS(a.reg.register_instance(dup, a.env), RegistryError, ErrorKind::STALE_INSTANCE_GENERATION);
}
void test_worker_restart_fresh_boot_wins() {
  A a;
  WorkerBootId b2 = a.reg.adopt_worker(WorkerId(1));  // restart
  CHECK(b2 != a.boot);
  CHECK(!a.reg.authority().is_boot_live(a.boot));
  // old boot cannot mutate current state
  AuthorityEnvelope old = a.env; old.boot = a.boot;
  CHECK_THROWS(a.reg.update_health(a.inst, Health::HEALTHY, old), RegistryError, ErrorKind::STALE_BOOT);
  // fresh boot registers a superseding incarnation with higher generation
  ServiceInstance ni; ni.service_id=a.service; ni.service_generation=ServiceGeneration(5);
  ni.instance_generation=ServiceInstanceGeneration(11); ni.node=NodeId(1); ni.worker=WorkerId(1); ni.boot=b2;
  ni.runtime_id=a.runtime; ni.runtime_instance=a.rtinst; ni.version=SemanticVersion::parse("1.0.0").value();
  ni.lifecycle=Lifecycle::REGISTERING;
  AuthorityEnvelope env2; env2.epoch=a.reg.authority().epoch; env2.boot=b2;
  ServiceInstanceId niid = a.reg.register_instance(ni, env2);
  CHECK(niid != a.inst);
  // old instance is superseded
  CHECK(a.reg.find_instance(a.inst)->lifecycle == Lifecycle::SUPERSEDED);
  // stale replay of old instance cannot resurrect (lower generation)
  AuthorityEnvelope env3; env3.epoch=a.reg.authority().epoch; env3.boot=b2;
  CHECK_THROWS(a.reg.register_instance(dup_inst(a.service, a.runtime, a.rtinst, 10, b2), env3), RegistryError, ErrorKind::STALE_INSTANCE_GENERATION);
}
ServiceInstance dup_inst(ServiceId sid, RuntimeId rid, RuntimeInstanceId ri, int gen, WorkerBootId boot) {
  ServiceInstance x; x.service_id=sid; x.service_generation=ServiceGeneration(5); x.instance_generation=ServiceInstanceGeneration(gen);
  x.node=NodeId(1); x.worker=WorkerId(1); x.boot=boot; x.runtime_id=rid; x.runtime_instance=ri;
  x.version=SemanticVersion::parse("1.0.0").value(); x.lifecycle=Lifecycle::REGISTERING; return x;
}
}  // namespace

void test_adversarial_suite();
void test_adversarial_suite() {
  test_stale_epoch(); test_stale_boot(); test_stale_health_update(); test_stale_readiness_update();
  test_stale_service_generation(); test_conflicting_duplicate_instance(); test_worker_restart_fresh_boot_wins();
}
RR_REGISTER(test_adversarial_suite);
int main() { return rr_test::run_all("adversarial"); }
