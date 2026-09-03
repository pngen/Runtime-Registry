#pragma once
// Canonical descriptor model for Runtime Registry.
// Each descriptor is a value type with explicit identity, generation,
// provenance, freshness, health, readiness, and authority fields.

#include <runtimeregistry/capability.hpp>
#include <runtimeregistry/enums.hpp>
#include <runtimeregistry/identity.hpp>
#include <runtimeregistry/provenance.hpp>
#include <runtimeregistry/version.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace runtimeregistry {

// ---- Endpoint --------------------------------------------------------------
// Cross-process endpoint authority is never a process-local pointer/address.
struct EndpointLocator {
  std::string text;   // e.g. "127.0.0.1:31817" (TCP) or named identity for IPC
  [[nodiscard]] bool empty() const noexcept { return text.empty(); }
  friend bool operator==(const EndpointLocator& a, const EndpointLocator& b) = default;
};

enum class EndpointSecurity { NONE, PLAINTEXT, UNKNOWN };
[[nodiscard]] std::string_view to_string(EndpointSecurity s) noexcept;

struct EndpointDescriptor {
  EndpointId endpoint_id;
  ServiceInstanceId service_instance;
  EndpointGeneration generation;
  ProtocolId protocol;
  EndpointLocator locator;
  TransportKind transport{TransportKind::UNKNOWN};
  std::uint16_t port{0};
  EndpointSecurity security{EndpointSecurity::UNKNOWN};
  Health health{Health::UNKNOWN};
  Reachability reachability{Reachability::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  Provenance provenance;
  AuthorityGeneration authority_generation;

  friend bool operator==(const EndpointDescriptor& a,
                         const EndpointDescriptor& b) = default;
};

// ---- Protocol --------------------------------------------------------------
struct ProtocolDescriptor {
  ProtocolId protocol_id;
  std::string name;
  ProtocolGeneration generation;
  ProtocolVersion version;
  ProtocolVersion compatibility_min;  // lowest compatible major/minor (major-compatible)
  std::string framing;                // e.g. "FRAMED_CRC32"
  std::vector<CapabilityKind> required_capabilities;
  std::int64_t max_payload{0};        // 0 => unbounded/unknown
  Provenance provenance;

  friend bool operator==(const ProtocolDescriptor& a,
                         const ProtocolDescriptor& b) = default;
};

// ---- Runtime ---------------------------------------------------------------
struct RuntimeDescriptor {
  RuntimeId runtime_id;
  RuntimeInstanceId runtime_instance;
  RuntimeGeneration generation;
  RuntimeKind kind{RuntimeKind::UNKNOWN};
  std::string name;
  std::string family;
  SemanticVersion version;
  ApiVersion api_version;
  AbiVersion abi_version;
  std::string build_identity;
  std::string compiler_identity;
  std::vector<ProtocolId> supported_protocols;
  std::vector<CapabilityId> capabilities;
  std::vector<BackendId> backends;
  std::string architecture;        // e.g. "sm_120", "x64"
  std::string device_constraint;   // optional constraint text
  Provenance provenance;
  NodeId node;
  WorkerId owner_worker;
  WorkerBootId owner_boot;

  friend bool operator==(const RuntimeDescriptor& a,
                         const RuntimeDescriptor& b) = default;
};

// ---- Backend ---------------------------------------------------------------
struct BackendDescriptor {
  BackendId backend_id;
  BackendKind kind{BackendKind::UNKNOWN};
  BackendGeneration generation;
  RuntimeInstanceId runtime_binding;   // which runtime instance provides it
  std::vector<CapabilityId> capabilities;
  SemanticVersion version;
  Health health{Health::UNKNOWN};
  Readiness readiness{Readiness::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  Provenance provenance;

  friend bool operator==(const BackendDescriptor& a,
                         const BackendDescriptor& b) = default;
};

// ---- Device ----------------------------------------------------------------
struct DeviceDescriptor {
  DeviceId device_id;
  DeviceGeneration generation;
  std::string name;
  std::string vendor;
  std::string architecture;        // sm target when established
  std::string compute_capability;  // e.g. "12.0" when established
  std::int64_t total_memory{0};    // bytes; 0 => unknown
  std::int64_t free_memory{0};
  std::string backend_binding;     // e.g. "CUDA"
  Provenance provenance;
  Freshness freshness{Freshness::UNKNOWN};
  Health health{Health::UNKNOWN};

  friend bool operator==(const DeviceDescriptor& a,
                         const DeviceDescriptor& b) = default;
};

// ---- Service ---------------------------------------------------------------
struct ServiceDescriptor {
  ServiceId service_id;
  ServiceKind kind{ServiceKind::UNKNOWN};
  std::string name;
  ServiceGeneration generation;
  OwnerId owner;
  Lifecycle lifecycle{Lifecycle::REGISTERING};
  SemanticVersion version;
  ApiVersion api_version;
  AbiVersion abi_version;
  std::vector<CapabilityKind> required_capabilities;
  std::vector<CapabilityKind> optional_capabilities;
  std::vector<ProtocolId> protocol_requirements;
  CompatibilityId compatibility_ref;   // optional
  PolicyGeneration policy_generation;
  Provenance provenance;
  std::string semantic_digest;

  friend bool operator==(const ServiceDescriptor& a,
                         const ServiceDescriptor& b) = default;
};

// ---- Service instance ------------------------------------------------------
struct ServiceInstance {
  ServiceInstanceId instance_id;
  ServiceId service_id;
  ServiceGeneration service_generation;
  ServiceInstanceGeneration instance_generation;
  NodeId node;
  WorkerId worker;
  WorkerBootId boot;
  ProcessId process;
  RuntimeId runtime_id;
  RuntimeInstanceId runtime_instance;
  std::vector<EndpointId> endpoints;
  std::vector<ProtocolId> protocols;
  std::vector<CapabilityId> capabilities;
  Health health{Health::UNKNOWN};
  Readiness readiness{Readiness::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  Reachability reachability{Reachability::UNKNOWN};
  LeaseId lease;
  Lifecycle lifecycle{Lifecycle::REGISTERING};
  SemanticVersion version;
  CompatibilityId compatibility_ref;
  AuthorityGeneration authority_generation;
  std::int64_t registered_ms{0};
  std::int64_t updated_ms{0};
  std::int64_t expires_ms{0};   // lease expiry, 0 => no wall-clock expiry
  Provenance provenance;
  std::string semantic_digest;

  friend bool operator==(const ServiceInstance& a,
                         const ServiceInstance& b) = default;
};

// ---- Node ------------------------------------------------------------------
struct NodeDescriptor {
  NodeId node_id;
  NodeGeneration generation;
  std::string hostname;
  std::vector<RuntimeInstanceId> runtimes;
  std::vector<ServiceInstanceId> service_instances;
  std::vector<DeviceId> devices;
  std::vector<BackendId> backends;
  Health health{Health::UNKNOWN};
  Readiness readiness{Readiness::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  Reachability reachability{Reachability::UNKNOWN};
  Provenance provenance;

  friend bool operator==(const NodeDescriptor& a,
                         const NodeDescriptor& b) = default;
};

// ---- Lease -----------------------------------------------------------------
enum class LeaseRenewalPolicy { FIXED, SESSION, CONNECTION, BOOT, MANUAL, UNKNOWN };
[[nodiscard]] std::string_view to_string(LeaseRenewalPolicy p) noexcept;

struct LeaseDescriptor {
  LeaseId lease_id;
  LeaseGeneration generation;
  ServiceInstanceId service_instance;
  WorkerBootId boot;
  std::int64_t issued_ms{0};
  std::int64_t renew_interval_ms{0};
  LeaseRenewalPolicy renewal_policy{LeaseRenewalPolicy::UNKNOWN};
  LeaseState state{LeaseState::UNKNOWN};
  std::int64_t expires_ms{0};
  Provenance provenance;
  AuthorityGeneration authority_generation;

  friend bool operator==(const LeaseDescriptor& a,
                         const LeaseDescriptor& b) = default;
};

// ---- Invalidation ----------------------------------------------------------
struct InvalidationRecord {
  std::string target_kind;    // "ServiceId", "ServiceInstanceId", etc.
  std::string target_text;    // rendered strong id / boot id
  RecordGeneration generation;   // generation floor for mutation rejection
  CoordinatorEpoch epoch;
  WorkerBootId boot;
  AuthorityGeneration authority_generation;
  std::string reason;
  std::int64_t timestamp_ms{0};
  Provenance provenance;

  friend bool operator==(const InvalidationRecord& a,
                         const InvalidationRecord& b) = default;
};

// ---- Tombstone -------------------------------------------------------------
struct TombstoneRecord {
  TombstoneId tombstone_id;
  std::string target_kind;    // identity kind of the covered target
  std::string target_text;    // rendered strong id / boot id
  RecordGeneration generation_floor;   // mutations at or below this are fenced
  CoordinatorEpoch epoch;
  WorkerBootId boot;
  AuthorityGeneration authority_generation;
  std::string reason;
  std::int64_t timestamp_ms{0};
  Provenance provenance;

  friend bool operator==(const TombstoneRecord& a,
                         const TombstoneRecord& b) = default;
};

// ---- Compatibility reference ----------------------------------------------
struct CompatibilityRecord {
  CompatibilityId compatibility_id;
  CompatibilityGeneration generation;
  CompatibilityOutcome outcome{CompatibilityOutcome::UNKNOWN};
  Freshness freshness{Freshness::UNKNOWN};
  Provenance provenance;

  friend bool operator==(const CompatibilityRecord& a,
                         const CompatibilityRecord& b) = default;
};

// ---- Dependency ------------------------------------------------------------
struct ServiceDependency {
  ServiceKind required_kind{ServiceKind::UNKNOWN};
  ServiceId required_service;   // optional, 0 => any of kind
  SemanticVersion minimum_version;
  CapabilityKind required_capability{CapabilityKind::UNKNOWN};
  bool required{true};
  RecordGeneration generation;

  friend bool operator==(const ServiceDependency& a,
                         const ServiceDependency& b) = default;
};

// ---- Authority envelope ----------------------------------------------------
// Every mutation carries a full authority envelope. If any component is stale
// relative to the registry's current authority, the mutation is rejected before
// state change.
struct AuthorityEnvelope {
  CoordinatorEpoch epoch;
  WorkerBootId boot;
  RegistryGeneration registry_generation;
  ServiceGeneration service_generation;
  ServiceInstanceGeneration service_instance_generation;
  RuntimeGeneration runtime_generation;
  RuntimeInstanceGeneration runtime_instance_generation;
  EndpointGeneration endpoint_generation;
  ProtocolGeneration protocol_generation;
  BackendGeneration backend_generation;
  DeviceGeneration device_generation;
  CapabilityGeneration capability_generation;
  LeaseGeneration lease_generation;
  ObservationGeneration observation_generation;
  AttemptGeneration attempt_generation;
  DispatchGeneration dispatch_generation;
  RecordGeneration record_generation;

  friend bool operator==(const AuthorityEnvelope& a,
                         const AuthorityEnvelope& b) = default;
};

}  // namespace runtimeregistry
