#pragma once
// Discovery query model, deterministic selection, and explanation data.

#include <runtimeregistry/capability.hpp>
#include <runtimeregistry/enums.hpp>
#include <runtimeregistry/identity.hpp>
#include <runtimeregistry/model.hpp>
#include <runtimeregistry/version.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace runtimeregistry {

// Named deterministic ranking factors (not an opaque master score).
enum class SelectionFactor {
  EXACT_SERVICE_ID,
  EXACT_PROTOCOL,
  EXACT_API_VERSION,
  EXACT_ABI,
  CAPABILITY_COMPLETENESS,
  READINESS,
  HEALTH,
  LOCALITY,
  RUNTIME_PREFERENCE,
  VERSION_RECENCY,
  ENDPOINT_REACHABILITY,
  FRESHNESS,
  POLICY_PREFERENCE,
};

[[nodiscard]] std::string_view to_string(SelectionFactor f) noexcept;

// Why a candidate was excluded by a hard filter.
enum class RejectionCategory {
  WRONG_KIND,
  WRONG_RUNTIME,
  STALE,
  INVALIDATED,
  TOMBSTONED,
  STALE_AUTHORITY,
  LEASE_INVALID,
  UNREADY,
  UNHEALTHY,
  UNREACHABLE,
  MISSING_CAPABILITY,
  INCOMPATIBLE_PROTOCOL,
  INCOMPATIBLE_API,
  INCOMPATIBLE_ABI,
  ARCHITECTURE_MISMATCH,
  BELOW_MIN_VERSION,
  INSUFFICIENT_EVIDENCE,
};

[[nodiscard]] std::string_view to_string(RejectionCategory r) noexcept;

struct RegistryQuery {
  ServiceKind service_kind{ServiceKind::UNKNOWN};   // UNKNOWN => any kind
  ServiceId exact_service_id;                       // 0 => any
  RuntimeKind runtime_kind{RuntimeKind::UNKNOWN};
  std::string runtime_family;
  ApiVersion minimum_api;
  bool has_minimum_api{false};
  AbiVersion abi_requirement;
  bool has_abi_requirement{false};
  ProtocolId protocol_requirement;                  // 0 => any
  ProtocolVersion protocol_version;                 // for negotiation
  bool has_protocol_version{false};
  std::vector<CapabilityKind> required_capabilities;
  BackendKind backend_requirement{BackendKind::UNKNOWN}; // UNKNOWN => any
  std::string architecture;
  Health required_health{Health::UNKNOWN};          // UNKNOWN => no filter
  Readiness required_readiness{Readiness::UNKNOWN}; // UNKNOWN => no filter
  Freshness required_freshness{Freshness::CURRENT}; // CURRENT => require current (but UNKNOWN means? see below)
  Reachability required_reachability{Reachability::REACHABLE}; // REACHABLE => require reachable
  NodeId preferred_node;                            // 0 => any
  RuntimeId preferred_runtime;                      // 0 => any
  VersionPolicy version_policy{VersionPolicy::PREFER_RECENT};
  bool current_only{true};
  std::size_t max_results{32};
};

// A scored candidate with named factor breakdown.
struct CandidateScore {
  ServiceInstanceId instance_id;
  std::vector<std::pair<SelectionFactor, double>> factors;
  double total{0.0};
  int rank{0};
};

// Per-candidate rejection explanation.
struct CandidateRejection {
  ServiceInstanceId instance_id;
  std::string reason;
  std::string detail;
  RejectionCategory category{RejectionCategory::INSUFFICIENT_EVIDENCE};
};

struct RegistryResult {
  QueryOutcome outcome{QueryOutcome::UNKNOWN};
  std::vector<CandidateScore> ranked;              // valid, ranked candidates
  std::vector<CandidateRejection> rejected;        // hard-filtered candidates
  std::vector<ServiceInstanceId> selected;         // top candidates (<= max_results)
  std::vector<std::pair<SelectionFactor, bool>> factor_summary;
  int total_considered{0};
  std::string explanation;
};

}  // namespace runtimeregistry
