#pragma once
// Shared helpers for the Runtime Registry examples. Each example builds against
// the real public library API (RuntimeRegistry::runtimeregistry).

#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/selection.hpp>
#include <runtimeregistry/persistence.hpp>
#include <cstdio>

namespace rr_examples {
using namespace runtimeregistry;

inline AuthorityEnvelope setup(Registry& reg, WorkerId worker = WorkerId(1)) {
  reg.begin_coordinator_epoch();
  WorkerBootId boot = reg.adopt_worker(worker);
  AuthorityEnvelope env; env.epoch = reg.authority().epoch; env.boot = boot;
  std::printf("  epoch=%llu boot=%llu\n", (unsigned long long)env.epoch.value(), (unsigned long long)boot.value());
  return env;
}

inline RuntimeInstanceId add_runtime(Registry& reg, const AuthorityEnvelope& env, const char* name,
                                     const char* family, const char* arch, RuntimeKind kind = RuntimeKind::NATIVE_CPP, std::uint64_t rt_id = 1) {
  RuntimeDescriptor rt; rt.runtime_id = RuntimeId(rt_id); rt.runtime_instance = RuntimeInstanceId(rt_id);
  rt.generation = RuntimeGeneration(1); rt.kind = kind; rt.name = name; rt.family = family;
  rt.version = SemanticVersion::parse("1.0.0").value(); rt.api_version = ApiVersion(1, 0); rt.abi_version = AbiVersion(1, 0);
  rt.architecture = arch; rt.provenance = make_provenance(EvidenceKind::REPORTED, name, 0);
  reg.register_runtime(rt, env); return rt.runtime_instance;
}

inline ServiceId add_service(Registry& reg, const AuthorityEnvelope& env, std::uint64_t id, ServiceKind kind,
                             const char* name, const SemanticVersion& ver, const ApiVersion& api,
                             std::vector<CapabilityKind> required = {}) {
  ServiceDescriptor svc; svc.service_id = ServiceId(id); svc.kind = kind; svc.name = name;
  svc.generation = ServiceGeneration(1); svc.version = ver; svc.api_version = api; svc.abi_version = AbiVersion(1, 0);
  svc.required_capabilities = std::move(required); svc.provenance = make_provenance(EvidenceKind::REPORTED, name, 0);
  reg.register_service(svc, env); return svc.service_id;
}

inline CapabilityId add_capability(Registry& reg, const AuthorityEnvelope& env, std::uint64_t id, CapabilityKind kind,
                                   CapabilityValue value, Freshness fresh = Freshness::CURRENT) {
  CapabilityDescriptor c; c.capability_id = CapabilityId(id); c.kind = kind; c.generation = CapabilityGeneration(1);
  c.version = SemanticVersion::parse("1.0.0").value(); c.value = std::move(value); c.freshness = fresh;
  c.provenance = make_provenance(EvidenceKind::MEASURED, "device", 0);
  reg.register_capability(c, env); return c.capability_id;
}

inline EndpointId add_endpoint(Registry& reg, const AuthorityEnvelope& env, std::uint64_t id, std::uint16_t port,
                               Reachability reach = Reachability::REACHABLE, TransportKind transport = TransportKind::TCP) {
  EndpointDescriptor ep; ep.endpoint_id = EndpointId(id); ep.generation = EndpointGeneration(1); ep.protocol = ProtocolId(1);
  ep.locator.text = "127.0.0.1:" + std::to_string(port); ep.transport = transport; ep.port = port;
  ep.health = Health::HEALTHY; ep.reachability = reach; ep.freshness = Freshness::CURRENT;
  ep.provenance = make_provenance(EvidenceKind::MEASURED, "probe", 0);
  reg.register_endpoint(ep, env); return ep.endpoint_id;
}

inline ServiceInstanceId add_instance(Registry& reg, const AuthorityEnvelope& env, ServiceId service, WorkerBootId boot,
                                      WorkerId worker, RuntimeInstanceId rt, NodeId node, const SemanticVersion& ver,
                                      std::vector<EndpointId> endpoints, std::vector<CapabilityId> caps,
                                      Health health = Health::HEALTHY, Readiness ready = Readiness::READY, std::uint64_t igen = 1) {
  ServiceInstance inst; inst.service_id = service; inst.service_generation = ServiceGeneration(1);
  inst.instance_generation = ServiceInstanceGeneration(igen); inst.node = node; inst.worker = worker; inst.boot = boot;
  inst.runtime_id = RuntimeId(1); inst.runtime_instance = rt; inst.endpoints = std::move(endpoints);
  inst.protocols = {ProtocolId(1)}; inst.capabilities = std::move(caps); inst.health = health; inst.readiness = ready;
  inst.freshness = Freshness::CURRENT; inst.reachability = Reachability::REACHABLE; inst.version = ver;
  inst.provenance = make_provenance(EvidenceKind::REPORTED, "worker", 0); inst.lifecycle = Lifecycle::REGISTERING;
  return reg.register_instance(inst, env);
}

inline void print(const RegistryResult& r) {
  std::printf("    outcome=%s ranked=%zu\n", to_string(r.outcome).data(), r.ranked.size());
  for (const auto& cs : r.ranked) std::printf("      instance=%llu total=%.1f\n", (unsigned long long)cs.instance_id.value(), cs.total);
}
}  // namespace rr_examples
