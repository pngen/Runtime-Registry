#pragma once
// Canonical runtime registry. Owns the service/runtime/capability discovery and
// authority boundary. All mutations are validated for authority before any
// state change; no partial current registration is ever committed.

#include <runtimeregistry/accounting.hpp>
#include <runtimeregistry/authority.hpp>
#include <runtimeregistry/capability.hpp>
#include <runtimeregistry/enums.hpp>
#include <runtimeregistry/error.hpp>
#include <runtimeregistry/identity.hpp>
#include <runtimeregistry/model.hpp>
#include <runtimeregistry/query.hpp>
#include <runtimeregistry/version.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace runtimeregistry {

struct RegistryOptions {
  bool retain_history{true};
  std::size_t max_history_per_identity{64};
  bool require_physical_probe_for_tcp{false};
};

// A parsed, committed authority verdict applied to a mutation.
struct AuthorityVerdict {
  bool accept{false};
  ErrorKind kind{ErrorKind::INTERNAL};
  std::string reason;
};

class Registry {
 public:
  Registry();
  explicit Registry(RegistryOptions opts);

  // ---- Coordinator / worker lifecycle -----------------------------------
  // Start a fresh epoch (coordinator boot / recovery). Live process authority
  // is cleared; existing logical metadata is preserved.
  void begin_coordinator_epoch();

  // Adopt a worker incarnation. Returns the fresh WorkerBootId. If the worker
  // already has a live boot, the new boot supersedes it (old boot revoked).
  WorkerBootId adopt_worker(WorkerId worker);

  // Mark a boot as died (OS process loss). The boot is revoked; any instance it
  // owned becomes STALE/UNREACHABLE as appropriate.
  void mark_worker_died(WorkerBootId boot);

  void set_worker_health_reporting(WorkerId worker, bool enabled);

  // ---- Registration / mutation -------------------------------------------
  // Each method validates the authority envelope before committing. On
  // rejection a RegistryError is thrown and no state changes.

  // Registers (or idempotently confirms) the Service contract.
  void register_service(const ServiceDescriptor& svc, const AuthorityEnvelope& env);

  // Registers a live ServiceInstance. Transactional: validates authority,
  // endpoints, protocols, capabilities, and lease before committing.
  ServiceInstanceId register_instance(const ServiceInstance& inst,
                                      const AuthorityEnvelope& env);

  RuntimeInstanceId register_runtime(const RuntimeDescriptor& rt,
                                     const AuthorityEnvelope& env);

  EndpointId register_endpoint(const EndpointDescriptor& ep,
                               const AuthorityEnvelope& env);

  CapabilityId register_capability(const CapabilityDescriptor& cap,
                                   const AuthorityEnvelope& env);

  BackendId register_backend(const BackendDescriptor& bd,
                             const AuthorityEnvelope& env);

  DeviceId register_device(const DeviceDescriptor& dd,
                           const AuthorityEnvelope& env);

  void register_protocol(const ProtocolDescriptor& pd, const AuthorityEnvelope& env);

  void register_node(const NodeDescriptor& nd, const AuthorityEnvelope& env);

  // ---- Instance transitions ----------------------------------------------
  void update_health(ServiceInstanceId id, Health h, const AuthorityEnvelope& env);
  void update_readiness(ServiceInstanceId id, Readiness r, const AuthorityEnvelope& env);
  void update_freshness(ServiceInstanceId id, Freshness f, const AuthorityEnvelope& env);
  void update_reachability(EndpointId id, Reachability r, const AuthorityEnvelope& env);
  void update_instance_reachability(ServiceInstanceId id, Reachability r,
                                    const AuthorityEnvelope& env);

  // ---- Leases ------------------------------------------------------------
  LeaseId acquire_lease(const LeaseDescriptor& lease, const AuthorityEnvelope& env);
  void renew_lease(LeaseId id, const AuthorityEnvelope& env);
  void expire_lease(LeaseId id, const AuthorityEnvelope& env);
  void revoke_lease(LeaseId id, const AuthorityEnvelope& env);
  void mark_lease_revalidation_required(LeaseId id);

  // ---- Invalidation / supersession / tombstone ---------------------------
  void invalidate(const InvalidationRecord& record, const AuthorityEnvelope& env);
  void supersede(ServiceInstanceId old_id, const ServiceInstance& new_inst,
                 const AuthorityEnvelope& env);
  TombstoneId create_tombstone(const TombstoneRecord& tomb, const AuthorityEnvelope& env);
  void deregister(ServiceInstanceId id, const AuthorityEnvelope& env);

  // ---- Discovery ---------------------------------------------------------
  RegistryResult query(const RegistryQuery& q) const;
  RegistryResult query_and_account(const RegistryQuery& q);

  // ---- Query explanation -------------------------------------------------
  std::string explain_query(const RegistryQuery& q) const;
  std::string explain_candidate(ServiceInstanceId id) const;
  std::string explain_rejection(ServiceInstanceId id, const RegistryQuery& q) const;
  std::string explain_service(ServiceId id) const;
  std::string explain_runtime(RuntimeId id) const;
  std::string explain_endpoint(EndpointId id) const;
  std::string explain_capability(CapabilityId id) const;
  std::string explain_version(const RegistryQuery& q, ServiceInstanceId id) const;
  std::string explain_protocol(ServiceInstanceId id, ProtocolId p) const;
  std::string explain_readiness(ServiceInstanceId id) const;
  std::string explain_reachability(ServiceInstanceId id) const;
  std::string explain_lease(LeaseId id) const;
  std::string explain_invalidation(ServiceInstanceId id) const;
  std::string explain_tombstone(const std::string& target_kind,
                                const std::string& target_text) const;
  std::string explain_recovery() const;

  // ---- Persistence -------------------------------------------------------
  std::vector<std::uint8_t> serialize() const;
  void serialize_to(std::vector<std::uint8_t>& out) const;
  // Throws RegistryError on invalid/corrupt input.
  void load_from(const std::uint8_t* data, std::size_t size);
  void load_from(const std::vector<std::uint8_t>& data);
  [[nodiscard]] std::string semantic_digest() const;

  // ---- State access ------------------------------------------------------
  [[nodiscard]] const RegistryAuthority& authority() const { return authority_; }
  [[nodiscard]] const Accounting& accounting() const { return accounting_; }
  [[nodiscard]] const RegistryOptions& options() const { return options_; }

  const ServiceInstance* find_instance(ServiceInstanceId id) const;
  const ServiceDescriptor* find_service(ServiceId id) const;
  const RuntimeDescriptor* find_runtime(RuntimeId id) const;
  const RuntimeDescriptor* find_runtime_instance(RuntimeInstanceId id) const;
  const EndpointDescriptor* find_endpoint(EndpointId id) const;
  const CapabilityDescriptor* find_capability(CapabilityId id) const;
  const LeaseDescriptor* find_lease(LeaseId id) const;

  // ---- Index / invariant validation --------------------------------------
  // Returns a non-empty list of invariant violations; empty means consistent.
  [[nodiscard]] std::vector<std::string> check_invariants() const;
  std::unordered_map<ServiceId, std::vector<ServiceInstanceId>> instance_index_by_service() const;

  // ---- accounting helpers ------------------------------------------------
  [[nodiscard]] bool accounting_negative() const;

  // ---- workers -----------------------------------------------------------
  [[nodiscard]] const std::unordered_map<WorkerId, WorkerBootId>& current_boots() const {
    return authority_.current_boot;
  }

 private:
  // Core validation used by every mutating method.
  AuthorityVerdict validate_envelope(const AuthorityEnvelope& env) const;
  // Validates that a mutation references the current generation of a record.
  bool generation_is_current_generation(const AuthorityEnvelope& env,
                                        const ServiceInstance& inst) const;
  // True if a current tombstone covers target_text / generation_floor.
  bool covered_by_tombstone(const std::string& target_kind,
                            const std::string& target_text,
                            std::uint64_t gen_floor) const;
  void reclassify_lifecycle(ServiceInstance& inst);
  void commit_registry_generation();
  void revoke_boot_internal(WorkerBootId boot);

  RegistryOptions options_;
  RegistryAuthority authority_;
  Accounting accounting_;

  // canonical current records
  std::unordered_map<ServiceId, ServiceDescriptor> services_;
  std::unordered_map<ServiceInstanceId, ServiceInstance> instances_;
  std::unordered_map<RuntimeId, RuntimeDescriptor> runtimes_;
  std::unordered_map<RuntimeInstanceId, RuntimeDescriptor> runtime_instances_;
  std::unordered_map<EndpointId, EndpointDescriptor> endpoints_;
  std::unordered_map<ProtocolId, ProtocolDescriptor> protocols_;
  std::unordered_map<BackendId, BackendDescriptor> backends_;
  std::unordered_map<DeviceId, DeviceDescriptor> devices_;
  std::unordered_map<CapabilityId, CapabilityDescriptor> capabilities_;
  std::unordered_map<NodeId, NodeDescriptor> nodes_;
  std::unordered_map<LeaseId, LeaseDescriptor> leases_;
  std::unordered_map<TombstoneId, TombstoneRecord> tombstones_;
  std::unordered_map<CompatibilityId, CompatibilityRecord> compat_records_;
  std::vector<InvalidationRecord> invalidations_;

  // history (current + retired/superseded/tombstoned instances)
  std::vector<ServiceInstance> instance_history_;
  std::vector<RuntimeDescriptor> runtime_history_;
  std::vector<EndpointDescriptor> endpoint_history_;
  std::vector<CapabilityDescriptor> capability_history_;

  // derived secondary indexes (verified against canonical records)
  std::unordered_map<ServiceKind, std::unordered_set<ServiceId>> index_by_kind_;
  std::unordered_map<ServiceId, std::unordered_set<ServiceInstanceId>> index_by_service_;
  std::unordered_map<WorkerBootId, std::unordered_set<ServiceInstanceId>> index_by_boot_;
  std::unordered_map<NodeId, std::unordered_set<ServiceInstanceId>> index_by_node_;

  std::uint64_t next_boot_;
  std::uint64_t next_id_;
  mutable std::recursive_mutex mtx_;

  void rebuild_indexes();
};

}  // namespace runtimeregistry
