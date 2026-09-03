#pragma once
// Enumeration of registry domains. Each state is a distinct named value with
// deterministic string conversions and explicit semantics. Unknown is always
// a valid value and never silently promoted to a known state.

#include <cstdint>
#include <string>
#include <string_view>

namespace runtimeregistry {

// ---- Health -----------------------------------------------------------------
enum class Health {
  HEALTHY,
  DEGRADED,
  UNHEALTHY,
  UNAVAILABLE,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(Health h) noexcept;

// ---- Readiness -------------------------------------------------------------
enum class Readiness {
  READY,
  PARTIALLY_READY,
  NOT_READY,
  DRAINING,
  REVALIDATION_REQUIRED,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(Readiness r) noexcept;

// ---- Freshness -------------------------------------------------------------
enum class Freshness {
  CURRENT,
  STALE,
  REVALIDATION_REQUIRED,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(Freshness f) noexcept;

// ---- Reachability ----------------------------------------------------------
enum class Reachability {
  REACHABLE,
  DEGRADED,
  UNREACHABLE,
  REVALIDATION_REQUIRED,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(Reachability r) noexcept;

// ---- Service/instance lifecycle -------------------------------------------
enum class Lifecycle {
  REGISTERING,
  AVAILABLE,
  DEGRADED,
  DRAINING,
  UNREADY,
  STALE,
  UNREACHABLE,
  INVALIDATED,
  SUPERSEDED,
  TOMBSTONED,
  RETIRED,
  FAILED,
};
[[nodiscard]] std::string_view to_string(Lifecycle l) noexcept;

// ---- Service kinds ---------------------------------------------------------
enum class ServiceKind {
  SCHEDULER,
  CACHE_DIRECTORY,
  STATE_INDEX,
  STORAGE_SERVICE,
  CHECKPOINT_SERVICE,
  RESOURCE_BROKER,
  HEALTH_AGENT,
  FLEET_AGENT,
  COMPILER_SERVICE,
  MODEL_SERVER,
  TRANSFER_SERVICE,
  COLLECTIVE_SERVICE,
  OBSERVABILITY_SERVICE,
  REGISTRY_SERVICE,
  EXECUTION_SERVICE,
  GENERIC_RUNTIME_SERVICE,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(ServiceKind k) noexcept;
[[nodiscard]] ServiceKind service_kind_from_string(std::string_view s) noexcept;

// ---- Runtime kinds ---------------------------------------------------------
enum class RuntimeKind {
  NATIVE_CPP,
  CUDA_RUNTIME,
  CUDA_DRIVER,
  CUSTOM_ACCELERATOR_RUNTIME,
  MODEL_RUNTIME,
  SERVING_RUNTIME,
  STORAGE_RUNTIME,
  NETWORK_RUNTIME,
  COMPILATION_RUNTIME,
  SYNTHETIC_RUNTIME,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(RuntimeKind k) noexcept;
[[nodiscard]] RuntimeKind runtime_kind_from_string(std::string_view s) noexcept;

// ---- Transport kinds -------------------------------------------------------
enum class TransportKind {
  TCP,
  LOCAL_IPC,
  SHARED_MEMORY,
  IN_PROCESS,
  SYNTHETIC_REMOTE,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(TransportKind t) noexcept;
[[nodiscard]] TransportKind transport_kind_from_string(std::string_view s) noexcept;

// ---- Backend kinds ---------------------------------------------------------
enum class BackendKind {
  CUDA,
  CPU,
  LOCAL_FILESYSTEM,
  TCP,
  SYNTHETIC_REMOTE,
  CUSTOM,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(BackendKind b) noexcept;
[[nodiscard]] BackendKind backend_kind_from_string(std::string_view s) noexcept;

// ---- Capability kinds ------------------------------------------------------
enum class CapabilityKind {
  CUDA_AVAILABLE,
  CUDA_COMPUTE_CAPABILITY,
  CUDA_ARCHITECTURE,
  CUDA_MEMORY,
  HOST_PINNED_MEMORY,
  LOCAL_FILESYSTEM,
  LOCAL_STORAGE,
  TCP_TRANSPORT,
  FRAMED_PROTOCOL,
  MULTIPROCESS,
  COMPILATION,
  PERSISTENCE,
  CHECKPOINTING,
  COLLECTIVES,
  REMOTE_ACCESS_CLASS,
  HEALTH_REPORTING,
  POWER_TELEMETRY,
  GENERIC_FEATURE,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(CapabilityKind k) noexcept;
[[nodiscard]] CapabilityKind capability_kind_from_string(std::string_view s) noexcept;

// ---- Protocol negotiation outcomes ----------------------------------------
enum class NegotiationOutcome {
  EXACT,
  COMPATIBLE,
  DOWNGRADE_ALLOWED,
  UPGRADE_REQUIRED,
  INCOMPATIBLE,
  INSUFFICIENT_EVIDENCE,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(NegotiationOutcome o) noexcept;

// ---- Query outcomes --------------------------------------------------------
enum class QueryOutcome {
  FOUND_EXACT,
  FOUND_COMPATIBLE,
  FOUND_MULTIPLE,
  NOT_FOUND,
  STALE_ONLY,
  UNREADY_ONLY,
  UNHEALTHY_ONLY,
  UNREACHABLE_ONLY,
  INCOMPATIBLE_ONLY,
  INSUFFICIENT_EVIDENCE,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(QueryOutcome o) noexcept;

// ---- Compatibility outcome -------------------------------------------------
enum class CompatibilityOutcome {
  COMPATIBLE,
  COMPATIBLE_WITH_DOWNGRADE,
  INCOMPATIBLE,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(CompatibilityOutcome c) noexcept;

// ---- Lease lifecycle -------------------------------------------------------
enum class LeaseState {
  ACTIVE,
  EXPIRED,
  REVOKED,
  REVALIDATION_REQUIRED,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(LeaseState s) noexcept;

// ---- Version selection policy ---------------------------------------------
enum class VersionPolicy {
  EXACT,
  MINIMUM,
  COMPATIBLE_MAJOR,
  BOUNDED_RANGE,
  PREFER_RECENT,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(VersionPolicy p) noexcept;

}  // namespace runtimeregistry
