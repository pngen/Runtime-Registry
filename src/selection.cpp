#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/selection.hpp>
#include <algorithm>
#include <sstream>

namespace runtimeregistry {

std::string_view to_string(SelectionFactor f) noexcept {
  switch (f) {
    case SelectionFactor::EXACT_SERVICE_ID: return "exact_service_id";
    case SelectionFactor::EXACT_PROTOCOL: return "exact_protocol";
    case SelectionFactor::EXACT_API_VERSION: return "exact_api_version";
    case SelectionFactor::EXACT_ABI: return "exact_abi";
    case SelectionFactor::CAPABILITY_COMPLETENESS: return "capability_completeness";
    case SelectionFactor::READINESS: return "readiness";
    case SelectionFactor::HEALTH: return "health";
    case SelectionFactor::LOCALITY: return "locality";
    case SelectionFactor::RUNTIME_PREFERENCE: return "runtime_preference";
    case SelectionFactor::VERSION_RECENCY: return "version_recency";
    case SelectionFactor::ENDPOINT_REACHABILITY: return "endpoint_reachability";
    case SelectionFactor::FRESHNESS: return "freshness";
    case SelectionFactor::POLICY_PREFERENCE: return "policy_preference";
  }
  return "unknown";
}

std::string_view to_string(RejectionCategory r) noexcept {
  switch (r) {
    case RejectionCategory::WRONG_KIND: return "wrong_service_kind";
    case RejectionCategory::WRONG_RUNTIME: return "wrong_runtime";
    case RejectionCategory::STALE: return "stale";
    case RejectionCategory::INVALIDATED: return "invalidated";
    case RejectionCategory::TOMBSTONED: return "tombstoned";
    case RejectionCategory::STALE_AUTHORITY: return "stale_authority";
    case RejectionCategory::LEASE_INVALID: return "lease_invalid";
    case RejectionCategory::UNREADY: return "unready";
    case RejectionCategory::UNHEALTHY: return "unhealthy";
    case RejectionCategory::UNREACHABLE: return "unreachable";
    case RejectionCategory::MISSING_CAPABILITY: return "missing_capability";
    case RejectionCategory::INCOMPATIBLE_PROTOCOL: return "incompatible_protocol";
    case RejectionCategory::INCOMPATIBLE_API: return "incompatible_api";
    case RejectionCategory::INCOMPATIBLE_ABI: return "incompatible_abi";
    case RejectionCategory::ARCHITECTURE_MISMATCH: return "architecture_mismatch";
    case RejectionCategory::BELOW_MIN_VERSION: return "below_min_version";
    case RejectionCategory::INSUFFICIENT_EVIDENCE: return "insufficient_evidence";
  }
  return "insufficient_evidence";
}

NegotiationOutcome negotiate_protocol(const ProtocolVersion& required, const ProtocolVersion& offered, bool exact_required) noexcept {
  if (required == offered) return NegotiationOutcome::EXACT;
  if (exact_required) return NegotiationOutcome::INCOMPATIBLE;
  if (required.major() != offered.major()) return NegotiationOutcome::INCOMPATIBLE;
  if (required.minor() == offered.minor()) return NegotiationOutcome::EXACT;
  if (offered.minor() > required.minor()) return NegotiationOutcome::UPGRADE_REQUIRED;
  return NegotiationOutcome::DOWNGRADE_ALLOWED;
}

VersionVerdict version_verdict(const SemanticVersion& candidate, VersionPolicy policy, const ApiVersion& min_api, const ApiVersion& candidate_api, bool has_min_api) noexcept {
  (void)candidate; (void)policy;
  VersionVerdict v;
  if (has_min_api && candidate_api < min_api) { v.outcome = NegotiationOutcome::INCOMPATIBLE; v.detail = "API version below minimum"; return v; }
  v.outcome = NegotiationOutcome::COMPATIBLE;
  return v;
}

int capability_satisfaction(const CapabilityDescriptor& cap, CapabilityKind required) noexcept {
  if (cap.kind != required) return 1;
  if (cap.freshness != Freshness::CURRENT) return 0;
  return -1;
}

std::string reject_detail(RejectionCategory cat) {
  switch (cat) {
    case RejectionCategory::WRONG_KIND: return "wrong service kind";
    case RejectionCategory::WRONG_RUNTIME: return "wrong runtime";
    case RejectionCategory::STALE: return "stale instance";
    case RejectionCategory::INVALIDATED: return "invalidated";
    case RejectionCategory::TOMBSTONED: return "tombstoned";
    case RejectionCategory::STALE_AUTHORITY: return "stale authority";
    case RejectionCategory::LEASE_INVALID: return "invalid lease";
    case RejectionCategory::UNREADY: return "not ready";
    case RejectionCategory::UNHEALTHY: return "not healthy";
    case RejectionCategory::UNREACHABLE: return "not reachable";
    case RejectionCategory::MISSING_CAPABILITY: return "missing required capability";
    case RejectionCategory::INCOMPATIBLE_PROTOCOL: return "incompatible protocol";
    case RejectionCategory::INCOMPATIBLE_API: return "incompatible API";
    case RejectionCategory::INCOMPATIBLE_ABI: return "incompatible ABI";
    case RejectionCategory::ARCHITECTURE_MISMATCH: return "architecture mismatch";
    case RejectionCategory::BELOW_MIN_VERSION: return "below minimum version";
    case RejectionCategory::INSUFFICIENT_EVIDENCE: return "insufficient evidence";
  }
  return "insufficient evidence";
}
namespace {

bool readiness_meets(Readiness actual, Readiness required) noexcept {
  if (required == Readiness::UNKNOWN) return true;
  if (required == Readiness::READY) return actual == Readiness::READY || actual == Readiness::PARTIALLY_READY;
  if (required == Readiness::DRAINING) return actual == Readiness::DRAINING;
  if (required == Readiness::PARTIALLY_READY) return actual == Readiness::PARTIALLY_READY || actual == Readiness::READY;
  return actual == required;
}

bool health_meets(Health actual, Health required) noexcept {
  if (required == Health::UNKNOWN) return true;
  if (required == Health::HEALTHY) return actual == Health::HEALTHY;
  if (required == Health::DEGRADED) return actual == Health::DEGRADED || actual == Health::HEALTHY;
  return actual == required;
}

bool reachability_meets(Reachability actual, Reachability required) noexcept {
  if (required == Reachability::UNKNOWN) return true;
  if (required == Reachability::REACHABLE) return actual == Reachability::REACHABLE || actual == Reachability::DEGRADED;
  if (required == Reachability::DEGRADED) return actual == Reachability::DEGRADED || actual == Reachability::REACHABLE;
  return actual == required;
}

bool freshness_meets(Freshness actual, Freshness required) noexcept {
  if (required == Freshness::UNKNOWN) return true;
  if (required == Freshness::CURRENT) return actual == Freshness::CURRENT;
  return actual == required;
}

}  // namespace
RegistryResult Registry::query(const RegistryQuery& q) const {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  RegistryResult result;
  result.total_considered = 0;
  bool saw_stale = false, saw_unready = false, saw_unhealthy = false, saw_unreachable = false, saw_incompatible = false, saw_insufficient = false, saw_any = false;
  for (const auto& kv : instances_) {
    const ServiceInstance& inst = kv.second;
    auto sres = services_.find(inst.service_id);
    if (sres == services_.end()) continue;
    const ServiceDescriptor& svc = sres->second;
    if (static_cast<bool>(q.exact_service_id) && inst.service_id != q.exact_service_id) continue;
    if (q.service_kind != ServiceKind::UNKNOWN && svc.kind != q.service_kind) { saw_any = true; saw_incompatible = true; continue; }
    ++result.total_considered;

    CandidateRejection rej;
    rej.instance_id = inst.instance_id;
    auto reject = [&](RejectionCategory cat, const std::string& detail) {
      rej.category = cat; rej.reason = reject_detail(cat); rej.detail = detail; result.rejected.push_back(rej);
      if (cat == RejectionCategory::STALE || cat == RejectionCategory::STALE_AUTHORITY || cat == RejectionCategory::LEASE_INVALID || cat == RejectionCategory::INVALIDATED || cat == RejectionCategory::TOMBSTONED) saw_stale = true;
      else if (cat == RejectionCategory::UNREADY) saw_unready = true;
      else if (cat == RejectionCategory::UNHEALTHY) saw_unhealthy = true;
      else if (cat == RejectionCategory::UNREACHABLE) saw_unreachable = true;
      else if (cat == RejectionCategory::INCOMPATIBLE_PROTOCOL || cat == RejectionCategory::INCOMPATIBLE_API || cat == RejectionCategory::INCOMPATIBLE_ABI || cat == RejectionCategory::BELOW_MIN_VERSION || cat == RejectionCategory::ARCHITECTURE_MISMATCH || cat == RejectionCategory::WRONG_RUNTIME) saw_incompatible = true;
      else if (cat == RejectionCategory::INSUFFICIENT_EVIDENCE) saw_insufficient = true;
      saw_any = true;
    };

    if (q.current_only && (inst.lifecycle == Lifecycle::SUPERSEDED || inst.lifecycle == Lifecycle::TOMBSTONED || inst.lifecycle == Lifecycle::RETIRED || inst.lifecycle == Lifecycle::INVALIDATED)) { reject(RejectionCategory::STALE, "historical instance excluded from current lookup"); continue; }
    if (inst.lifecycle == Lifecycle::STALE) { reject(RejectionCategory::STALE, "instance is stale"); continue; }
    if (inst.lifecycle == Lifecycle::INVALIDATED) { reject(RejectionCategory::INVALIDATED, "instance invalidated"); continue; }
    if (inst.lifecycle == Lifecycle::TOMBSTONED) { reject(RejectionCategory::TOMBSTONED, "instance tombstoned"); continue; }
    if (!authority_.is_boot_live(inst.boot)) { reject(RejectionCategory::STALE_AUTHORITY, "instance owned by a non-live incarnation"); continue; }
    if (!freshness_meets(inst.freshness, q.required_freshness)) { reject(RejectionCategory::STALE, "freshness not accepted"); continue; }
    if (static_cast<bool>(inst.lease)) { auto lit = leases_.find(inst.lease); if (lit == leases_.end() || lit->second.state != LeaseState::ACTIVE) { reject(RejectionCategory::LEASE_INVALID, "lease not active"); continue; } }
    if (!readiness_meets(inst.readiness, q.required_readiness)) { reject(RejectionCategory::UNREADY, "readiness not accepted"); continue; }
    if (!health_meets(inst.health, q.required_health)) { reject(RejectionCategory::UNHEALTHY, "health not accepted"); continue; }
    if (!reachability_meets(inst.reachability, q.required_reachability)) { reject(RejectionCategory::UNREACHABLE, "reachability not accepted"); continue; }

    if (static_cast<bool>(q.protocol_requirement) && q.has_protocol_version) {
      bool any_proto = false;
      for (ProtocolId pid : inst.protocols) {
        auto pit = protocols_.find(pid);
        if (pit == protocols_.end()) continue;
        any_proto = true;
        NegotiationOutcome o = negotiate_protocol(q.protocol_version, pit->second.version, false);
        if (o == NegotiationOutcome::INCOMPATIBLE) { reject(RejectionCategory::INCOMPATIBLE_PROTOCOL, "no compatible protocol"); goto done_proto; }
      }
      if (!any_proto) { reject(RejectionCategory::INSUFFICIENT_EVIDENCE, "protocol evidence insufficient"); continue; }
      done_proto: ;
    }

    bool cap_invalid = false;
    for (CapabilityKind req : q.required_capabilities) {
      int sat = 1;
      for (CapabilityId cid : inst.capabilities) { auto cit = capabilities_.find(cid); if (cit == capabilities_.end()) continue; if (capability_satisfaction(cit->second, req) == -1) { sat = -1; break; } if (capability_satisfaction(cit->second, req) == 0) sat = 0; }
      if (sat == -1) continue;
      if (sat == 0) { reject(RejectionCategory::INSUFFICIENT_EVIDENCE, "required capability " + std::string(to_string(req)) + " is UNKNOWN"); cap_invalid = true; break; }
      reject(RejectionCategory::MISSING_CAPABILITY, "required capability " + std::string(to_string(req)) + " missing"); cap_invalid = true; break;
    }
    if (cap_invalid) continue;
    const RuntimeDescriptor* rt = nullptr;
    auto rit = runtime_instances_.find(inst.runtime_instance);
    if (rit != runtime_instances_.end()) rt = &rit->second;

    if (rt != nullptr) {
      if (q.runtime_kind != RuntimeKind::UNKNOWN && rt->kind != q.runtime_kind) { reject(RejectionCategory::WRONG_RUNTIME, "wrong runtime kind"); continue; }
      if (!q.runtime_family.empty() && rt->family != q.runtime_family) { reject(RejectionCategory::WRONG_RUNTIME, "wrong runtime family"); continue; }
      if (!q.architecture.empty() && !rt->architecture.empty() && rt->architecture != q.architecture) { reject(RejectionCategory::ARCHITECTURE_MISMATCH, "architecture mismatch"); continue; }
    }
    if (q.has_minimum_api && svc.api_version < q.minimum_api) { reject(RejectionCategory::BELOW_MIN_VERSION, "API below minimum"); continue; }
    if (q.has_abi_requirement && svc.abi_version.major() != q.abi_requirement.major()) { reject(RejectionCategory::INCOMPATIBLE_ABI, "ABI major mismatch"); continue; }

    CandidateScore cs;
    cs.instance_id = inst.instance_id;
    auto addf = [&](SelectionFactor f, double v) { cs.factors.push_back({f, v}); cs.total += v; };
    addf(SelectionFactor::EXACT_SERVICE_ID, (q.exact_service_id && inst.service_id == q.exact_service_id) ? 1000.0 : 0.0);
    addf(SelectionFactor::EXACT_API_VERSION, (q.has_minimum_api && svc.api_version == q.minimum_api) ? 200.0 : 0.0);
    addf(SelectionFactor::EXACT_ABI, (q.has_abi_requirement && svc.abi_version == q.abi_requirement) ? 150.0 : (q.has_abi_requirement && svc.abi_version.major() == q.abi_requirement.major() ? 80.0 : 0.0));
    double capn = 0;
    for (CapabilityKind req2 : q.required_capabilities) { int sat2 = 1; for (CapabilityId cid2 : inst.capabilities) { auto cit2 = capabilities_.find(cid2); if (cit2 == capabilities_.end()) continue; if (capability_satisfaction(cit2->second, req2) == -1) { sat2 = -1; break; } } if (sat2 == -1) capn += 1.0; }
    addf(SelectionFactor::CAPABILITY_COMPLETENESS, q.required_capabilities.empty() ? 100.0 : (capn / static_cast<double>(q.required_capabilities.size()) * 100.0));
    addf(SelectionFactor::READINESS, inst.readiness == Readiness::READY ? 80.0 : (inst.readiness == Readiness::PARTIALLY_READY ? 40.0 : 0.0));
    addf(SelectionFactor::HEALTH, inst.health == Health::HEALTHY ? 80.0 : (inst.health == Health::DEGRADED ? 40.0 : 0.0));
    addf(SelectionFactor::LOCALITY, (q.preferred_node && inst.node == q.preferred_node) ? 100.0 : 0.0);
    addf(SelectionFactor::RUNTIME_PREFERENCE, (q.preferred_runtime && inst.runtime_id == q.preferred_runtime) ? 100.0 : 0.0);
    addf(SelectionFactor::VERSION_RECENCY, static_cast<double>(svc.version.major * 100 + svc.version.minor * 10 + svc.version.patch));
    addf(SelectionFactor::ENDPOINT_REACHABILITY, inst.reachability == Reachability::REACHABLE ? 60.0 : (inst.reachability == Reachability::DEGRADED ? 30.0 : 0.0));
    addf(SelectionFactor::FRESHNESS, inst.freshness == Freshness::CURRENT ? 100.0 : 0.0);
    addf(SelectionFactor::POLICY_PREFERENCE, q.version_policy == VersionPolicy::PREFER_RECENT ? 10.0 : 0.0);
    result.ranked.push_back(cs);
    saw_any = true;
  }

  std::sort(result.ranked.begin(), result.ranked.end(), [](const CandidateScore& a, const CandidateScore& b) { if (a.total != b.total) return a.total > b.total; return a.instance_id < b.instance_id; });
  for (std::size_t i = 0; i < result.ranked.size(); ++i) result.ranked[i].rank = static_cast<int>(i);
  for (const CandidateScore& cs : result.ranked) result.selected.push_back(cs.instance_id);
  if (result.selected.size() > q.max_results) result.selected.resize(q.max_results);

  if (!result.ranked.empty()) {
    if (result.ranked.size() == 1) result.outcome = QueryOutcome::FOUND_EXACT;
    else result.outcome = QueryOutcome::FOUND_MULTIPLE;
  } else if (!saw_any) { result.outcome = QueryOutcome::NOT_FOUND; }
  else if (saw_insufficient && !saw_stale && !saw_unready && !saw_unhealthy && !saw_unreachable && !saw_incompatible) { result.outcome = QueryOutcome::INSUFFICIENT_EVIDENCE; }
  else if (saw_stale) { result.outcome = QueryOutcome::STALE_ONLY; }
  else if (saw_unready) { result.outcome = QueryOutcome::UNREADY_ONLY; }
  else if (saw_unhealthy) { result.outcome = QueryOutcome::UNHEALTHY_ONLY; }
  else if (saw_unreachable) { result.outcome = QueryOutcome::UNREACHABLE_ONLY; }
  else if (saw_incompatible) { result.outcome = QueryOutcome::INCOMPATIBLE_ONLY; }
  else { result.outcome = QueryOutcome::INSUFFICIENT_EVIDENCE; }
  return result;
}

RegistryResult Registry::query_and_account(const RegistryQuery& q) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  RegistryResult r = query(q);
  ++accounting_.queries;
  if (r.outcome == QueryOutcome::FOUND_EXACT) ++accounting_.exact_hits;
  else if (r.outcome == QueryOutcome::FOUND_MULTIPLE || r.outcome == QueryOutcome::FOUND_COMPATIBLE) ++accounting_.compatible_hits;
  else if (r.outcome == QueryOutcome::NOT_FOUND) ++accounting_.misses;
  else if (r.outcome == QueryOutcome::STALE_ONLY) ++accounting_.stale_only;
  else if (r.outcome == QueryOutcome::UNREADY_ONLY) ++accounting_.unready_only;
  else if (r.outcome == QueryOutcome::INCOMPATIBLE_ONLY) ++accounting_.incompatible;
  return r;
}

}  // namespace runtimeregistry