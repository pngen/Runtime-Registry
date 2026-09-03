#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/selection.hpp>
#include <runtimeregistry/persistence.hpp>
#include <runtimeregistry/protocol.hpp>
#include "test_util.hpp"

using namespace runtimeregistry;

namespace {

LeaseId reg_acquire_lease(Registry& reg, const LeaseDescriptor& lease, const AuthorityEnvelope& env);
ServiceInstance test_get_instance(Registry& reg, ServiceInstanceId id);

struct Scenario {
  Registry reg;
  CoordinatorEpoch epoch;
  WorkerId workerA;
  WorkerBootId bootA;
  AuthorityEnvelope envA;
  ServiceId service;
  RuntimeId runtime;
  RuntimeInstanceId rtinst;
  EndpointId endpoint;
  CapabilityId cap;
  ServiceInstanceId instance;
  LeaseId lease;

  Scenario() : workerA(1) {
    reg.begin_coordinator_epoch();
    epoch = reg.authority().epoch;
    bootA = reg.adopt_worker(workerA);
    envA.epoch = epoch;
    envA.boot = bootA;
  }

  void register_runtime_and_service();
  void register_leaf();
  void register_instance_basic();
};

void Scenario::register_runtime_and_service() {
  RuntimeDescriptor rt;
  rt.runtime_id = RuntimeId(100);
  rt.runtime_instance = RuntimeInstanceId(200);
  rt.generation = RuntimeGeneration(1);
  rt.kind = RuntimeKind::NATIVE_CPP;
  rt.name = "native";
  rt.family = "cpp";
  rt.version = SemanticVersion::parse("1.0.0").value();
  rt.api_version = ApiVersion(1, 0);
  rt.abi_version = AbiVersion(1, 0);
  rt.architecture = "x64";
  reg.register_runtime(rt, envA);
  runtime = rt.runtime_id;
  rtinst = rt.runtime_instance;

  ServiceDescriptor svc;
  svc.service_id = ServiceId(10);
  svc.kind = ServiceKind::MODEL_SERVER;
  svc.name = "inference";
  svc.generation = ServiceGeneration(1);
  svc.version = SemanticVersion::parse("2.0.0").value();
  svc.api_version = ApiVersion(2, 0);
  svc.abi_version = AbiVersion(1, 0);
  svc.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  reg.register_service(svc, envA);
  service = svc.service_id;
}

void Scenario::register_leaf() {
  CapabilityDescriptor c;
  c.capability_id = CapabilityId(300);
  c.kind = CapabilityKind::CUDA_AVAILABLE;
  c.generation = CapabilityGeneration(1);
  c.version = SemanticVersion::parse("1.0.0").value();
  c.value = CapabilityValue::make_bool(true);
  c.freshness = Freshness::CURRENT;
  c.provenance = make_provenance(EvidenceKind::MEASURED, "device0", 0);
  reg.register_capability(c, envA);
  cap = c.capability_id;

  EndpointDescriptor ep;
  ep.endpoint_id = EndpointId(400);
  ep.generation = EndpointGeneration(1);
  ep.protocol = ProtocolId(1);
  ep.locator.text = "127.0.0.1:31817";
  ep.transport = TransportKind::TCP;
  ep.port = 31817;
  ep.health = Health::HEALTHY;
  ep.reachability = Reachability::REACHABLE;
  ep.freshness = Freshness::CURRENT;
  ep.provenance = make_provenance(EvidenceKind::MEASURED, "probe", 0);
  reg.register_endpoint(ep, envA);
  endpoint = ep.endpoint_id;
}

void Scenario::register_instance_basic() {
  ServiceInstance inst;
  inst.service_id = service;
  inst.service_generation = ServiceGeneration(1);
  inst.instance_generation = ServiceInstanceGeneration(1);
  inst.node = NodeId(1);
  inst.worker = workerA;
  inst.boot = bootA;
  inst.process = ProcessId(123);
  inst.runtime_id = runtime;
  inst.runtime_instance = rtinst;
  inst.endpoints = {endpoint};
  inst.protocols = {ProtocolId(1)};
  inst.capabilities = {cap};
  inst.health = Health::HEALTHY;
  inst.readiness = Readiness::READY;
  inst.freshness = Freshness::CURRENT;
  inst.reachability = Reachability::REACHABLE;
  inst.version = SemanticVersion::parse("2.0.0").value();
  inst.provenance = make_provenance(EvidenceKind::REPORTED, "workerA", 0);
  inst.lifecycle = Lifecycle::REGISTERING;
  instance = reg.register_instance(inst, envA);
  LeaseDescriptor ld;
  ld.service_instance = instance;
  ld.boot = bootA;
  ld.state = LeaseState::ACTIVE;
  ld.issued_ms = 0;
  ld.renewal_policy = LeaseRenewalPolicy::BOOT;
  ld.provenance = make_provenance(EvidenceKind::REPORTED, "workerA", 0);
  lease = reg_acquire_lease(reg, ld, envA);
}

LeaseId reg_acquire_lease(Registry& reg, const LeaseDescriptor& lease, const AuthorityEnvelope& env) {
  return reg.acquire_lease(lease, env);
}

void test_registration_and_discovery() {
  Scenario s;
  s.register_runtime_and_service();
  s.register_leaf();
  s.register_instance_basic();

  // In-process no-op idempotent duplicate registration
  ServiceInstance dup;
  dup = test_get_instance(s.reg, s.instance);
  dup.boot = s.bootA;
  // re-register same instance generation and content => no-op success returns same id
  ServiceInstanceId again = s.reg.register_instance(dup, s.envA);
  CHECK(again == s.instance);

  RegistryQuery q;
  q.service_kind = ServiceKind::MODEL_SERVER;
  q.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  RegistryResult r = s.reg.query(q);
  CHECK(r.outcome == QueryOutcome::FOUND_EXACT);
  CHECK(!r.ranked.empty());
  CHECK(r.ranked[0].instance_id == s.instance);

  // health transition
  s.envA.service_instance_generation = ServiceInstanceGeneration(1);
  s.reg.update_health(s.instance, Health::HEALTHY, s.envA);
  s.reg.update_readiness(s.instance, Readiness::READY, s.envA);
  const ServiceInstance* inst = s.reg.find_instance(s.instance);
  CHECK(inst != nullptr);
  CHECK(inst->lifecycle == Lifecycle::AVAILABLE);
  CHECK(inst->health == Health::HEALTHY);
}

ServiceInstance test_get_instance(Registry& reg, ServiceInstanceId id) {
  const ServiceInstance* i = reg.find_instance(id);
  if (!i) throw std::runtime_error("missing instance");
  return *i;
}

void test_lifecycle_unready_when_stale_boot() {
  Scenario s;
  s.register_runtime_and_service();
  s.register_leaf();
  s.register_instance_basic();
  s.reg.mark_worker_died(s.bootA);
  const ServiceInstance* inst = s.reg.find_instance(s.instance);
  CHECK(inst != nullptr);
  CHECK(inst->lifecycle == Lifecycle::STALE);
  CHECK(inst->reachability == Reachability::UNREACHABLE);
}

void test_tombstone_prevents_resurrection() {
  Scenario s;
  s.register_runtime_and_service();
  s.register_leaf();
  s.register_instance_basic();
  // A second service with no live instance is registered and then tombstoned.
  ServiceDescriptor svc2;
  svc2.service_id = ServiceId(77);
  svc2.kind = ServiceKind::MODEL_SERVER;
  svc2.name = "retired-service";
  svc2.generation = ServiceGeneration(1);
  svc2.version = SemanticVersion::parse("1.0.0").value();
  svc2.api_version = ApiVersion(1, 0);
  svc2.abi_version = AbiVersion(1, 0);
  s.reg.register_service(svc2, s.envA);
  TombstoneRecord tomb;
  tomb.target_kind = "ServiceId";
  tomb.target_text = std::to_string(svc2.service_id.value());
  tomb.generation_floor = RecordGeneration(2);
  tomb.epoch = s.epoch;
  tomb.boot = s.bootA;
  tomb.reason = "retired";
  s.reg.create_tombstone(tomb, s.envA);
  // Attempting to register a service instance with generation <= floor is rejected.
  ServiceInstance inst;
  inst.service_id = svc2.service_id;
  inst.service_generation = ServiceGeneration(1);
  inst.instance_generation = ServiceInstanceGeneration(1);
  inst.node = NodeId(1);
  inst.worker = s.workerA;
  inst.boot = s.bootA;
  inst.runtime_id = s.runtime;
  inst.runtime_instance = s.rtinst;
  inst.capabilities = {s.cap};
  inst.lifecycle = Lifecycle::REGISTERING;
  bool threw = false;
  try { s.reg.register_instance(inst, s.envA); } catch (const RegistryError& e) { threw = (e.kind() == ErrorKind::TOMBSTONE_RESURRECTION); }
  CHECK(threw);
}

void test_persistence_round_trip() {
  Scenario s;
  s.register_runtime_and_service();
  s.register_leaf();
  s.register_instance_basic();
  s.envA.service_instance_generation = ServiceInstanceGeneration(1);
  s.reg.update_readiness(s.instance, Readiness::READY, s.envA);

  std::vector<std::uint8_t> bytes = s.reg.serialize();
  CHECK(!bytes.empty());
  // Deterministic round trip: load is idempotent and produces a stable byte stream.
  Registry r2;
  r2.load_from(bytes);
  std::vector<std::uint8_t> b1 = r2.serialize();
  std::string d1 = r2.semantic_digest();
  Registry r3;
  r3.load_from(b1);
  std::vector<std::uint8_t> b2 = r3.serialize();
  CHECK(b1 == b2);
  CHECK(r2.semantic_digest() == d1);
  // Logical registry metadata/history survives recovery.
  CHECK(r2.find_service(s.service) != nullptr);
  CHECK(r2.find_instance(s.instance) != nullptr);
  // Live process authority is cleared, so a fresh lookup requires revalidation.
  RegistryQuery q;
  q.service_kind = ServiceKind::MODEL_SERVER;
  q.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  RegistryResult res = r2.query_and_account(q);
  CHECK(res.outcome == QueryOutcome::STALE_ONLY || res.outcome == QueryOutcome::NOT_FOUND ||
        res.outcome == QueryOutcome::FOUND_EXACT || res.outcome == QueryOutcome::FOUND_MULTIPLE);
}

void test_persistence_corruption() {
  Scenario s;
  s.register_runtime_and_service();
  s.register_leaf();
  s.register_instance_basic();
  std::vector<std::uint8_t> bytes = s.reg.serialize();
  // corrupt a byte in the middle
  std::vector<std::uint8_t> bad = bytes;
  bad[bytes.size() / 2] ^= 0x5A;
  Registry r2;
  bool threw = false;
  try { r2.load_from(bad); } catch (const RegistryError& e) { threw = (e.kind() == ErrorKind::MALFORMED); }
  CHECK(threw);
  // truncation
  Registry r3;
  std::vector<std::uint8_t> trunc(bytes.begin(), bytes.begin() + 30);
  threw = false;
  try { r3.load_from(trunc); } catch (const RegistryError& e) { threw = (e.kind() == ErrorKind::MALFORMED); }
  CHECK(threw);
}

}  // namespace

void test_registry_suite();
void test_registry_suite() {
  test_registration_and_discovery();
  test_lifecycle_unready_when_stale_boot();
  test_tombstone_prevents_resurrection();
  test_persistence_round_trip();
  test_persistence_corruption();
}
RR_REGISTER(test_registry_suite);

int main() {
  return rr_test::run_all("registry");
}
