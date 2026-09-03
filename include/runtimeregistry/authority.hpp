#pragma once
// Authority state and validation. Authority is incarnation-scoped: a larger
// generation from an old WorkerBootId must never fence a fresh incarnation.

#include <runtimeregistry/identity.hpp>
#include <runtimeregistry/model.hpp>

#include <unordered_map>
#include <unordered_set>

namespace runtimeregistry {

struct TrustGenerationTag;
using TrustGeneration = Gen<TrustGenerationTag>;

// Live authority state for a registry.
struct RegistryAuthority {
  CoordinatorEpoch epoch;
  RegistryGeneration registry_generation;
  RecordGeneration record_generation;

  // per-worker current incarnation
  std::unordered_map<WorkerId, WorkerBootId> current_boot;
  std::unordered_set<WorkerBootId> live_boots;
  std::unordered_map<WorkerBootId, WorkerId> boot_worker;

  // per-domain monotonic generation counters
  ServiceGeneration service_gen;
  ServiceInstanceGeneration instance_gen;
  RuntimeGeneration runtime_gen;
  RuntimeInstanceGeneration runtime_instance_gen;
  EndpointGeneration endpoint_gen;
  ProtocolGeneration protocol_gen;
  BackendGeneration backend_gen;
  DeviceGeneration device_gen;
  CapabilityGeneration capability_gen;
  LeaseGeneration lease_gen;
  NodeGeneration node_gen;
  CompatibilityGeneration compat_gen;
  TrustGeneration trust_gen;
  AuthorityGeneration authority_gen;

  [[nodiscard]] bool is_boot_live(WorkerBootId boot) const noexcept {
    return live_boots.count(boot) != 0;
  }

  // Returns the WorkerId owning a boot, or 0 if unknown.
  [[nodiscard]] WorkerId boot_owner(WorkerBootId boot) const noexcept {
    auto it = boot_worker.find(boot);
    return it == boot_worker.end() ? WorkerId{} : it->second;
  }
};

}  // namespace runtimeregistry
