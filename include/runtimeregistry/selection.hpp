#pragma once
// Deterministic candidate filtering and ranking helpers for discovery queries.

#include <runtimeregistry/enums.hpp>
#include <runtimeregistry/identity.hpp>
#include <runtimeregistry/model.hpp>
#include <runtimeregistry/query.hpp>
#include <runtimeregistry/version.hpp>

#include <optional>
#include <string>
#include <vector>

namespace runtimeregistry {

// Protocol negotiation outcome given a required version and a candidate's
// offered version. Never silently negotiates UNKNOWN to COMPATIBLE.
[[nodiscard]] NegotiationOutcome negotiate_protocol(
    const ProtocolVersion& required, const ProtocolVersion& offered,
    bool exact_required) noexcept;

// Structured version compatibility given a policy.
struct VersionVerdict {
  NegotiationOutcome outcome{NegotiationOutcome::UNKNOWN};
  std::string detail;
};
[[nodiscard]] VersionVerdict version_verdict(const SemanticVersion& candidate,
                                             VersionPolicy policy,
                                             const ApiVersion& min_api,
                                             const ApiVersion& candidate_api,
                                             bool has_min_api) noexcept;

// Capability satisfaction: UNKNOWN never satisfies a required capability.
// -1 => present, 0 => absent/unknown, 1 => absent (definitely).
[[nodiscard]] int capability_satisfaction(const CapabilityDescriptor& cap,
                                          CapabilityKind required) noexcept;

// Renders a deterministic explanation string for a rejection category.
[[nodiscard]] std::string reject_detail(RejectionCategory cat);

}  // namespace runtimeregistry
