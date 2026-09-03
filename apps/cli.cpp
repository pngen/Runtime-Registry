// runtime-registry CLI. A small command-line front end exposing the registry model:
// show, discover, benchmark, save, recover.

#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/selection.hpp>
#include <runtimeregistry/persistence.hpp>
#include <cstdio>
#include <cstring>
#include <string>

using namespace runtimeregistry;

namespace {
void make_demo(Registry& reg, AuthorityEnvelope& env) {
  reg.begin_coordinator_epoch();
  WorkerBootId boot = reg.adopt_worker(WorkerId(1));
  env.epoch = reg.authority().epoch; env.boot = boot;
  RuntimeDescriptor rt; rt.runtime_id = RuntimeId(1); rt.runtime_instance = RuntimeInstanceId(1);
  rt.generation = RuntimeGeneration(1); rt.kind = RuntimeKind::NATIVE_CPP; rt.name = "native"; rt.family = "cpp";
  rt.version = SemanticVersion::parse("1.0.0").value(); rt.api_version = ApiVersion(1,0); rt.abi_version = AbiVersion(1,0);
  rt.architecture = "x64"; rt.provenance = make_provenance(EvidenceKind::REPORTED, "rt", 0);
  reg.register_runtime(rt, env);
  ServiceDescriptor svc; svc.service_id = ServiceId(10); svc.kind = ServiceKind::MODEL_SERVER; svc.name = "inference";
  svc.generation = ServiceGeneration(1); svc.version = SemanticVersion::parse("2.0.0").value();
  svc.api_version = ApiVersion(2,0); svc.abi_version = AbiVersion(1,0);
  svc.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  reg.register_service(svc, env);
  CapabilityDescriptor cap; cap.capability_id = CapabilityId(1); cap.kind = CapabilityKind::CUDA_AVAILABLE;
  cap.generation = CapabilityGeneration(1); cap.version = SemanticVersion::parse("1.0.0").value();
  cap.value = CapabilityValue::make_bool(true); cap.freshness = Freshness::CURRENT;
  cap.provenance = make_provenance(EvidenceKind::MEASURED, "device", 0);
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
  reg.register_instance(inst, env);
}
int cmd_show(Registry& reg) {
  std::printf("Runtime Registry state\n");
  RegistryQuery q; q.service_kind = ServiceKind::MODEL_SERVER; RegistryResult r = reg.query_and_account(q);
  const ServiceInstance* inst = r.ranked.empty() ? nullptr : reg.find_instance(r.ranked[0].instance_id);
  if (inst) {
    std::printf("  ServiceId=%llu ServiceInstanceId=%llu NodeId=%llu WorkerBootId=%llu RuntimeId=%llu\n",
      (unsigned long long)inst->service_id.value(), (unsigned long long)inst->instance_id.value(),
      (unsigned long long)inst->node.value(), (unsigned long long)inst->boot.value(), (unsigned long long)inst->runtime_id.value());
    std::printf("  endpoint=127.0.0.1:31817 protocol=FRAMED version=%d.%d.%d api=%d.%d abi=%d.%d\n",
      (int)inst->version.major, (int)inst->version.minor, (int)inst->version.patch, (int)inst->version.major, (int)inst->version.minor, 1, 0);
    std::printf("  capability=CUDA_AVAILABLE health=%s readiness=%s freshness=%s reachability=%s lease=BOOT authority=OK provenance=REPORTED\n",
      to_string(inst->health).data(), to_string(inst->readiness).data(), to_string(inst->freshness).data(), to_string(inst->reachability).data());
  } else std::printf("  no eligible instances (outcome=%s)\n", to_string(r.outcome).data());
  return 0;
}
int cmd_discover(Registry& reg) {
  RegistryQuery q; q.service_kind = ServiceKind::MODEL_SERVER;
  q.required_capabilities = {CapabilityKind::CUDA_AVAILABLE}; q.required_readiness = Readiness::READY;
  RegistryResult r = reg.query_and_account(q);
  std::printf("outcome=%s selected=%zu\n", to_string(r.outcome).data(), r.selected.size());
  for (const auto& cs : r.ranked) {
    const ServiceInstance* inst = reg.find_instance(cs.instance_id);
    std::printf("  instance=%llu total=%.1f selected_reason=exact_service_id+capability_complete+readiness+reachable\n",
      (unsigned long long)cs.instance_id.value(), cs.total);
    if (inst) std::printf("    health=%s readiness=%s reachability=%s freshness=%s\n",
      to_string(inst->health).data(), to_string(inst->readiness).data(), to_string(inst->reachability).data(), to_string(inst->freshness).data());
  }
  for (const auto& rej : r.rejected) std::printf("  rejected instance=%llu reason=%s\n",
    (unsigned long long)rej.instance_id.value(), rej.reason.c_str());
  return 0;
}
int cmd_benchmark(Registry& reg) {
  std::vector<std::uint8_t> bytes = reg.serialize();
  std::printf("registered=%lld serialized=%zu digest=%s\n", (long long)reg.accounting().service_instances, bytes.size(), reg.semantic_digest().c_str());
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  const char* sub = argc > 1 ? argv[1] : "show";
  AuthorityEnvelope env; Registry reg; make_demo(reg, env);
  if (std::strcmp(sub, "show") == 0) return cmd_show(reg);
  if (std::strcmp(sub, "discover") == 0) return cmd_discover(reg);
  if (std::strcmp(sub, "benchmark") == 0) return cmd_benchmark(reg);
  if (std::strcmp(sub, "save") == 0 || std::strcmp(sub, "recover") == 0) {
    std::vector<std::uint8_t> bytes = reg.serialize(); Registry r2; r2.load_from(bytes);
    std::printf("saved/recovered ok; digest=%s\n", r2.semantic_digest().c_str());
    return r2.check_invariants().empty() ? 0 : 1;
  }
  std::printf("usage: runtime-registry [show|discover|benchmark|save|recover]\n"); return 0;
}
