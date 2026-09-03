#include <runtimeregistry/enums.hpp>
#include <runtimeregistry/model.hpp>

#include <array>
#include <string>

namespace runtimeregistry {

namespace {
template <typename E, std::size_t N>
E enum_from(std::string_view s, const std::array<std::pair<E, const char*>, N>& table,
            E fallback) {
  for (const auto& [e, name] : table) {
    if (s == name) return e;
  }
  return fallback;
}
}  // namespace

#define RR_STR_ENUM(ENUM) std::string_view to_string(ENUM value) noexcept { switch (value) {
#define RR_CASE(en, name) case en: return name;
#define RR_END } return "UNKNOWN"; }

RR_STR_ENUM(Health)
  RR_CASE(Health::HEALTHY, "HEALTHY")
  RR_CASE(Health::DEGRADED, "DEGRADED")
  RR_CASE(Health::UNHEALTHY, "UNHEALTHY")
  RR_CASE(Health::UNAVAILABLE, "UNAVAILABLE")
  RR_CASE(Health::UNKNOWN, "UNKNOWN")
RR_END

RR_STR_ENUM(Readiness)
  RR_CASE(Readiness::READY, "READY")
  RR_CASE(Readiness::PARTIALLY_READY, "PARTIALLY_READY")
  RR_CASE(Readiness::NOT_READY, "NOT_READY")
  RR_CASE(Readiness::DRAINING, "DRAINING")
  RR_CASE(Readiness::REVALIDATION_REQUIRED, "REVALIDATION_REQUIRED")
  RR_CASE(Readiness::UNKNOWN, "UNKNOWN")
RR_END

RR_STR_ENUM(Freshness)
  RR_CASE(Freshness::CURRENT, "CURRENT")
  RR_CASE(Freshness::STALE, "STALE")
  RR_CASE(Freshness::REVALIDATION_REQUIRED, "REVALIDATION_REQUIRED")
  RR_CASE(Freshness::UNKNOWN, "UNKNOWN")
RR_END

RR_STR_ENUM(Reachability)
  RR_CASE(Reachability::REACHABLE, "REACHABLE")
  RR_CASE(Reachability::DEGRADED, "DEGRADED")
  RR_CASE(Reachability::UNREACHABLE, "UNREACHABLE")
  RR_CASE(Reachability::REVALIDATION_REQUIRED, "REVALIDATION_REQUIRED")
  RR_CASE(Reachability::UNKNOWN, "UNKNOWN")
RR_END

RR_STR_ENUM(Lifecycle)
  RR_CASE(Lifecycle::REGISTERING, "REGISTERING")
  RR_CASE(Lifecycle::AVAILABLE, "AVAILABLE")
  RR_CASE(Lifecycle::DEGRADED, "DEGRADED")
  RR_CASE(Lifecycle::DRAINING, "DRAINING")
  RR_CASE(Lifecycle::UNREADY, "UNREADY")
  RR_CASE(Lifecycle::STALE, "STALE")
  RR_CASE(Lifecycle::UNREACHABLE, "UNREACHABLE")
  RR_CASE(Lifecycle::INVALIDATED, "INVALIDATED")
  RR_CASE(Lifecycle::SUPERSEDED, "SUPERSEDED")
  RR_CASE(Lifecycle::TOMBSTONED, "TOMBSTONED")
  RR_CASE(Lifecycle::RETIRED, "RETIRED")
  RR_CASE(Lifecycle::FAILED, "FAILED")
RR_END

// ServiceKind used by name for query serialization; keep explicit.

std::string_view to_string(ServiceKind k) noexcept {
  switch (k) {
    case ServiceKind::SCHEDULER: return "SCHEDULER";
    case ServiceKind::CACHE_DIRECTORY: return "CACHE_DIRECTORY";
    case ServiceKind::STATE_INDEX: return "STATE_INDEX";
    case ServiceKind::STORAGE_SERVICE: return "STORAGE_SERVICE";
    case ServiceKind::CHECKPOINT_SERVICE: return "CHECKPOINT_SERVICE";
    case ServiceKind::RESOURCE_BROKER: return "RESOURCE_BROKER";
    case ServiceKind::HEALTH_AGENT: return "HEALTH_AGENT";
    case ServiceKind::FLEET_AGENT: return "FLEET_AGENT";
    case ServiceKind::COMPILER_SERVICE: return "COMPILER_SERVICE";
    case ServiceKind::MODEL_SERVER: return "MODEL_SERVER";
    case ServiceKind::TRANSFER_SERVICE: return "TRANSFER_SERVICE";
    case ServiceKind::COLLECTIVE_SERVICE: return "COLLECTIVE_SERVICE";
    case ServiceKind::OBSERVABILITY_SERVICE: return "OBSERVABILITY_SERVICE";
    case ServiceKind::REGISTRY_SERVICE: return "REGISTRY_SERVICE";
    case ServiceKind::EXECUTION_SERVICE: return "EXECUTION_SERVICE";
    case ServiceKind::GENERIC_RUNTIME_SERVICE: return "GENERIC_RUNTIME_SERVICE";
    case ServiceKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(RuntimeKind k) noexcept {
  switch (k) {
    case RuntimeKind::NATIVE_CPP: return "NATIVE_CPP";
    case RuntimeKind::CUDA_RUNTIME: return "CUDA_RUNTIME";
    case RuntimeKind::CUDA_DRIVER: return "CUDA_DRIVER";
    case RuntimeKind::CUSTOM_ACCELERATOR_RUNTIME: return "CUSTOM_ACCELERATOR_RUNTIME";
    case RuntimeKind::MODEL_RUNTIME: return "MODEL_RUNTIME";
    case RuntimeKind::SERVING_RUNTIME: return "SERVING_RUNTIME";
    case RuntimeKind::STORAGE_RUNTIME: return "STORAGE_RUNTIME";
    case RuntimeKind::NETWORK_RUNTIME: return "NETWORK_RUNTIME";
    case RuntimeKind::COMPILATION_RUNTIME: return "COMPILATION_RUNTIME";
    case RuntimeKind::SYNTHETIC_RUNTIME: return "SYNTHETIC_RUNTIME";
    case RuntimeKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(TransportKind t) noexcept {
  switch (t) {
    case TransportKind::TCP: return "TCP";
    case TransportKind::LOCAL_IPC: return "LOCAL_IPC";
    case TransportKind::SHARED_MEMORY: return "SHARED_MEMORY";
    case TransportKind::IN_PROCESS: return "IN_PROCESS";
    case TransportKind::SYNTHETIC_REMOTE: return "SYNTHETIC_REMOTE";
    case TransportKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(BackendKind b) noexcept {
  switch (b) {
    case BackendKind::CUDA: return "CUDA";
    case BackendKind::CPU: return "CPU";
    case BackendKind::LOCAL_FILESYSTEM: return "LOCAL_FILESYSTEM";
    case BackendKind::TCP: return "TCP";
    case BackendKind::SYNTHETIC_REMOTE: return "SYNTHETIC_REMOTE";
    case BackendKind::CUSTOM: return "CUSTOM";
    case BackendKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(CapabilityKind k) noexcept {
  switch (k) {
    case CapabilityKind::CUDA_AVAILABLE: return "CUDA_AVAILABLE";
    case CapabilityKind::CUDA_COMPUTE_CAPABILITY: return "CUDA_COMPUTE_CAPABILITY";
    case CapabilityKind::CUDA_ARCHITECTURE: return "CUDA_ARCHITECTURE";
    case CapabilityKind::CUDA_MEMORY: return "CUDA_MEMORY";
    case CapabilityKind::HOST_PINNED_MEMORY: return "HOST_PINNED_MEMORY";
    case CapabilityKind::LOCAL_FILESYSTEM: return "LOCAL_FILESYSTEM";
    case CapabilityKind::LOCAL_STORAGE: return "LOCAL_STORAGE";
    case CapabilityKind::TCP_TRANSPORT: return "TCP_TRANSPORT";
    case CapabilityKind::FRAMED_PROTOCOL: return "FRAMED_PROTOCOL";
    case CapabilityKind::MULTIPROCESS: return "MULTIPROCESS";
    case CapabilityKind::COMPILATION: return "COMPILATION";
    case CapabilityKind::PERSISTENCE: return "PERSISTENCE";
    case CapabilityKind::CHECKPOINTING: return "CHECKPOINTING";
    case CapabilityKind::COLLECTIVES: return "COLLECTIVES";
    case CapabilityKind::REMOTE_ACCESS_CLASS: return "REMOTE_ACCESS_CLASS";
    case CapabilityKind::HEALTH_REPORTING: return "HEALTH_REPORTING";
    case CapabilityKind::POWER_TELEMETRY: return "POWER_TELEMETRY";
    case CapabilityKind::GENERIC_FEATURE: return "GENERIC_FEATURE";
    case CapabilityKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(NegotiationOutcome o) noexcept {
  switch (o) {
    case NegotiationOutcome::EXACT: return "EXACT";
    case NegotiationOutcome::COMPATIBLE: return "COMPATIBLE";
    case NegotiationOutcome::DOWNGRADE_ALLOWED: return "DOWNGRADE_ALLOWED";
    case NegotiationOutcome::UPGRADE_REQUIRED: return "UPGRADE_REQUIRED";
    case NegotiationOutcome::INCOMPATIBLE: return "INCOMPATIBLE";
    case NegotiationOutcome::INSUFFICIENT_EVIDENCE: return "INSUFFICIENT_EVIDENCE";
    case NegotiationOutcome::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(QueryOutcome o) noexcept {
  switch (o) {
    case QueryOutcome::FOUND_EXACT: return "FOUND_EXACT";
    case QueryOutcome::FOUND_COMPATIBLE: return "FOUND_COMPATIBLE";
    case QueryOutcome::FOUND_MULTIPLE: return "FOUND_MULTIPLE";
    case QueryOutcome::NOT_FOUND: return "NOT_FOUND";
    case QueryOutcome::STALE_ONLY: return "STALE_ONLY";
    case QueryOutcome::UNREADY_ONLY: return "UNREADY_ONLY";
    case QueryOutcome::UNHEALTHY_ONLY: return "UNHEALTHY_ONLY";
    case QueryOutcome::UNREACHABLE_ONLY: return "UNREACHABLE_ONLY";
    case QueryOutcome::INCOMPATIBLE_ONLY: return "INCOMPATIBLE_ONLY";
    case QueryOutcome::INSUFFICIENT_EVIDENCE: return "INSUFFICIENT_EVIDENCE";
    case QueryOutcome::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(CompatibilityOutcome c) noexcept {
  switch (c) {
    case CompatibilityOutcome::COMPATIBLE: return "COMPATIBLE";
    case CompatibilityOutcome::COMPATIBLE_WITH_DOWNGRADE: return "COMPATIBLE_WITH_DOWNGRADE";
    case CompatibilityOutcome::INCOMPATIBLE: return "INCOMPATIBLE";
    case CompatibilityOutcome::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(LeaseState s) noexcept {
  switch (s) {
    case LeaseState::ACTIVE: return "ACTIVE";
    case LeaseState::EXPIRED: return "EXPIRED";
    case LeaseState::REVOKED: return "REVOKED";
    case LeaseState::REVALIDATION_REQUIRED: return "REVALIDATION_REQUIRED";
    case LeaseState::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(VersionPolicy p) noexcept {
  switch (p) {
    case VersionPolicy::EXACT: return "EXACT";
    case VersionPolicy::MINIMUM: return "MINIMUM";
    case VersionPolicy::COMPATIBLE_MAJOR: return "COMPATIBLE_MAJOR";
    case VersionPolicy::BOUNDED_RANGE: return "BOUNDED_RANGE";
    case VersionPolicy::PREFER_RECENT: return "PREFER_RECENT";
    case VersionPolicy::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

// ---- from_string -----------------------------------------------------------
ServiceKind service_kind_from_string(std::string_view s) noexcept {
  static constexpr std::array<std::pair<ServiceKind, const char*>, 16> table = {{
    {ServiceKind::SCHEDULER, "SCHEDULER"},
    {ServiceKind::CACHE_DIRECTORY, "CACHE_DIRECTORY"},
    {ServiceKind::STATE_INDEX, "STATE_INDEX"},
    {ServiceKind::STORAGE_SERVICE, "STORAGE_SERVICE"},
    {ServiceKind::CHECKPOINT_SERVICE, "CHECKPOINT_SERVICE"},
    {ServiceKind::RESOURCE_BROKER, "RESOURCE_BROKER"},
    {ServiceKind::HEALTH_AGENT, "HEALTH_AGENT"},
    {ServiceKind::FLEET_AGENT, "FLEET_AGENT"},
    {ServiceKind::COMPILER_SERVICE, "COMPILER_SERVICE"},
    {ServiceKind::MODEL_SERVER, "MODEL_SERVER"},
    {ServiceKind::TRANSFER_SERVICE, "TRANSFER_SERVICE"},
    {ServiceKind::COLLECTIVE_SERVICE, "COLLECTIVE_SERVICE"},
    {ServiceKind::OBSERVABILITY_SERVICE, "OBSERVABILITY_SERVICE"},
    {ServiceKind::REGISTRY_SERVICE, "REGISTRY_SERVICE"},
    {ServiceKind::EXECUTION_SERVICE, "EXECUTION_SERVICE"},
    {ServiceKind::GENERIC_RUNTIME_SERVICE, "GENERIC_RUNTIME_SERVICE"},
  }};
  return enum_from(s, table, ServiceKind::UNKNOWN);
}

RuntimeKind runtime_kind_from_string(std::string_view s) noexcept {
  static constexpr std::array<std::pair<RuntimeKind, const char*>, 11> table = {{
    {RuntimeKind::NATIVE_CPP, "NATIVE_CPP"},
    {RuntimeKind::CUDA_RUNTIME, "CUDA_RUNTIME"},
    {RuntimeKind::CUDA_DRIVER, "CUDA_DRIVER"},
    {RuntimeKind::CUSTOM_ACCELERATOR_RUNTIME, "CUSTOM_ACCELERATOR_RUNTIME"},
    {RuntimeKind::MODEL_RUNTIME, "MODEL_RUNTIME"},
    {RuntimeKind::SERVING_RUNTIME, "SERVING_RUNTIME"},
    {RuntimeKind::STORAGE_RUNTIME, "STORAGE_RUNTIME"},
    {RuntimeKind::NETWORK_RUNTIME, "NETWORK_RUNTIME"},
    {RuntimeKind::COMPILATION_RUNTIME, "COMPILATION_RUNTIME"},
    {RuntimeKind::SYNTHETIC_RUNTIME, "SYNTHETIC_RUNTIME"},
    {RuntimeKind::UNKNOWN, "UNKNOWN"},
  }};
  return enum_from(s, table, RuntimeKind::UNKNOWN);
}

TransportKind transport_kind_from_string(std::string_view s) noexcept {
  static constexpr std::array<std::pair<TransportKind, const char*>, 6> table = {{
    {TransportKind::TCP, "TCP"},
    {TransportKind::LOCAL_IPC, "LOCAL_IPC"},
    {TransportKind::SHARED_MEMORY, "SHARED_MEMORY"},
    {TransportKind::IN_PROCESS, "IN_PROCESS"},
    {TransportKind::SYNTHETIC_REMOTE, "SYNTHETIC_REMOTE"},
    {TransportKind::UNKNOWN, "UNKNOWN"},
  }};
  return enum_from(s, table, TransportKind::UNKNOWN);
}

BackendKind backend_kind_from_string(std::string_view s) noexcept {
  static constexpr std::array<std::pair<BackendKind, const char*>, 7> table = {{
    {BackendKind::CUDA, "CUDA"},
    {BackendKind::CPU, "CPU"},
    {BackendKind::LOCAL_FILESYSTEM, "LOCAL_FILESYSTEM"},
    {BackendKind::TCP, "TCP"},
    {BackendKind::SYNTHETIC_REMOTE, "SYNTHETIC_REMOTE"},
    {BackendKind::CUSTOM, "CUSTOM"},
    {BackendKind::UNKNOWN, "UNKNOWN"},
  }};
  return enum_from(s, table, BackendKind::UNKNOWN);
}

CapabilityKind capability_kind_from_string(std::string_view s) noexcept {
  static constexpr std::array<std::pair<CapabilityKind, const char*>, 18> table = {{
    {CapabilityKind::CUDA_AVAILABLE, "CUDA_AVAILABLE"},
    {CapabilityKind::CUDA_COMPUTE_CAPABILITY, "CUDA_COMPUTE_CAPABILITY"},
    {CapabilityKind::CUDA_ARCHITECTURE, "CUDA_ARCHITECTURE"},
    {CapabilityKind::CUDA_MEMORY, "CUDA_MEMORY"},
    {CapabilityKind::HOST_PINNED_MEMORY, "HOST_PINNED_MEMORY"},
    {CapabilityKind::LOCAL_FILESYSTEM, "LOCAL_FILESYSTEM"},
    {CapabilityKind::LOCAL_STORAGE, "LOCAL_STORAGE"},
    {CapabilityKind::TCP_TRANSPORT, "TCP_TRANSPORT"},
    {CapabilityKind::FRAMED_PROTOCOL, "FRAMED_PROTOCOL"},
    {CapabilityKind::MULTIPROCESS, "MULTIPROCESS"},
    {CapabilityKind::COMPILATION, "COMPILATION"},
    {CapabilityKind::PERSISTENCE, "PERSISTENCE"},
    {CapabilityKind::CHECKPOINTING, "CHECKPOINTING"},
    {CapabilityKind::COLLECTIVES, "COLLECTIVES"},
    {CapabilityKind::REMOTE_ACCESS_CLASS, "REMOTE_ACCESS_CLASS"},
    {CapabilityKind::HEALTH_REPORTING, "HEALTH_REPORTING"},
    {CapabilityKind::POWER_TELEMETRY, "POWER_TELEMETRY"},
    {CapabilityKind::GENERIC_FEATURE, "GENERIC_FEATURE"},
  }};
  return enum_from(s, table, CapabilityKind::UNKNOWN);
}

std::string_view to_string(EndpointSecurity s) noexcept {
  switch (s) {
    case EndpointSecurity::NONE: return "NONE";
    case EndpointSecurity::PLAINTEXT: return "PLAINTEXT";
    case EndpointSecurity::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string_view to_string(LeaseRenewalPolicy p) noexcept {
  switch (p) {
    case LeaseRenewalPolicy::FIXED: return "FIXED";
    case LeaseRenewalPolicy::SESSION: return "SESSION";
    case LeaseRenewalPolicy::CONNECTION: return "CONNECTION";
    case LeaseRenewalPolicy::BOOT: return "BOOT";
    case LeaseRenewalPolicy::MANUAL: return "MANUAL";
    case LeaseRenewalPolicy::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

}  // namespace runtimeregistry
