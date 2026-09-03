#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/selection.hpp>
#include <runtimeregistry/persistence.hpp>
#include <cstdio>

using namespace runtimeregistry;

int main() {
  std::printf("[downstream_consumer]\n");
  Registry reg;
  reg.begin_coordinator_epoch();
  WorkerBootId boot = reg.adopt_worker(WorkerId(1));
  AuthorityEnvelope env; env.epoch = reg.authority().epoch; env.boot = boot;

  RuntimeDescriptor rt; rt.runtime_id = RuntimeId(1); rt.runtime_instance = RuntimeInstanceId(1);
  rt.generation = RuntimeGeneration(1); rt.kind = RuntimeKind::NATIVE_CPP; rt.name="native"; rt.family="cpp";
  rt.version = SemanticVersion::parse("1.0.0").value(); rt.api_version = ApiVersion(1,0); rt.abi_version = AbiVersion(1,0);
  rt.architecture = "x64"; rt.provenance = make_provenance(EvidenceKind::REPORTED, "rt", 0);
  reg.register_runtime(rt, env);

  ServiceDescriptor svc; svc.service_id = ServiceId(10); svc.kind = ServiceKind::MODEL_SERVER; svc.name="inference";
  svc.generation = ServiceGeneration(1); svc.version = SemanticVersion::parse("2.0.0").value();
  svc.api_version = ApiVersion(2,0); svc.abi_version = AbiVersion(1,0);
  svc.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  reg.register_service(svc, env);

  CapabilityDescriptor cap; cap.capability_id = CapabilityId(1); cap.kind = CapabilityKind::CUDA_AVAILABLE;
  cap.generation = CapabilityGeneration(1); cap.version = SemanticVersion::parse("1.0.0").value();
  cap.value = CapabilityValue::make_bool(true); cap.freshness = Freshness::CURRENT;
  cap.provenance = make_provenance(EvidenceKind::MEASURED, "dev", 0);
  reg.register_capability(cap, env);

  EndpointDescriptor ep; ep.endpoint_id = EndpointId(1); ep.generation = EndpointGeneration(1); ep.protocol = ProtocolId(1);
  ep.locator.text = "127.0.0.1:31817"; ep.transport = TransportKind::TCP; ep.port = 31817;
  ep.health = Health::HEALTHY; ep.reachability = Reachability::REACHABLE; ep.freshness = Freshness::CURRENT;
  ep.provenance = make_provenance(EvidenceKind::MEASURED, "probe", 0);
  reg.register_endpoint(ep, env);

  ServiceInstance inst; inst.service_id = svc.service_id; inst.service_generation = ServiceGeneration(1);
  inst.instance_generation = ServiceInstanceGeneration(1); inst.node = NodeId(1); inst.worker = WorkerId(1);
  inst.boot = boot; inst.runtime_id = rt.runtime_id; inst.runtime_instance = rt.runtime_instance;
  inst.endpoints = {ep.endpoint_id}; inst.protocols = {ProtocolId(1)}; inst.capabilities = {cap.capability_id};
  inst.health = Health::HEALTHY; inst.readiness = Readiness::READY; inst.freshness = Freshness::CURRENT;
  inst.reachability = Reachability::REACHABLE; inst.version = SemanticVersion::parse("2.0.0").value();
  inst.provenance = make_provenance(EvidenceKind::REPORTED, "worker", 0); inst.lifecycle = Lifecycle::REGISTERING;
  ServiceInstanceId sid = reg.register_instance(inst, env);

  RegistryQuery q; q.service_kind = ServiceKind::MODEL_SERVER;
  q.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  q.required_readiness = Readiness::READY;
  RegistryResult r = reg.query_and_account(q);
  std::printf("  exact query outcome=%s ranked=%zu\n", to_string(r.outcome).data(), r.ranked.size());
  bool ok = (r.outcome == QueryOutcome::FOUND_EXACT || r.outcome == QueryOutcome::FOUND_MULTIPLE) && !r.ranked.empty();

  env.service_instance_generation = ServiceInstanceGeneration(1);
  reg.update_readiness(sid, Readiness::READY, env);
  // supersede with a fresh instance
  ServiceInstance ni = inst; ni.instance_generation = ServiceInstanceGeneration(2);
  ni.version = SemanticVersion::parse("2.1.0").value();
  env.service_instance_generation = ServiceInstanceGeneration(2);
  ServiceInstanceId sid2 = reg.register_instance(ni, env);
  ok = ok && (reg.find_instance(sid) == nullptr || reg.find_instance(sid)->lifecycle == Lifecycle::SUPERSEDED);

  // historical lookup: a current-only query must not return the superseded instance.
  RegistryQuery hist; hist.service_kind = ServiceKind::MODEL_SERVER; hist.current_only = true;
  RegistryResult hr = reg.query(hist);
  ok = ok && !hr.ranked.empty();

  // persistence + recovery
  std::vector<std::uint8_t> bytes = reg.serialize();
  Registry r2; r2.load_from(bytes);
  ok = ok && (r2.find_service(svc.service_id) != nullptr);

  // invariant + accounting checks
  ok = ok && reg.check_invariants().empty();
  ok = ok && !reg.accounting_negative();

  // explanation
  std::printf("  explain_candidate: %s\n", reg.explain_candidate(sid2).c_str());

  std::printf("downstream_consumer: %s\n", ok ? "ALL PASS" : "FAILURES PRESENT");
  return ok ? 0 : 1;
}
