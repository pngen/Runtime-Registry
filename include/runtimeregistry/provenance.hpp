#pragma once
// Explicit provenance for every registry observation.
// Evidence allows consumers to distinguish measured physical facts from
// reported, derived, or synthetic facts, and to leave unknown facts unknown.

#include <cstdint>
#include <string>
#include <string_view>

namespace runtimeregistry {

enum class EvidenceKind {
  MEASURED,   // directly observed by this host (e.g. real cudaMalloc, loopback TCP)
  REPORTED,   // supplied by an actor on its own behalf
  DERIVED,    // computed from other evidence
  SYNTHETIC,  // deterministic synthetic scenario, not physical
  UNKNOWN,    // not established
};

[[nodiscard]] std::string_view to_string(EvidenceKind k) noexcept;
[[nodiscard]] EvidenceKind evidence_kind_from_string(std::string_view s) noexcept;

struct Provenance {
  EvidenceKind kind{EvidenceKind::UNKNOWN};
  std::string source;      // component/host that produced the observation
  std::int64_t timestamp_ms{0};  // epoch milliseconds
  std::string digest;      // optional stable semantic digest of the fact

  [[nodiscard]] bool is_physical() const noexcept {
    return kind == EvidenceKind::MEASURED || kind == EvidenceKind::REPORTED ||
           kind == EvidenceKind::DERIVED;
  }

  friend bool operator==(const Provenance& a, const Provenance& b) {
    return a.kind == b.kind && a.source == b.source &&
           a.timestamp_ms == b.timestamp_ms && a.digest == b.digest;
  }
  friend bool operator!=(const Provenance& a, const Provenance& b) {
    return !(a == b);
  }
};

// Convenience factory.
[[nodiscard]] inline Provenance make_provenance(EvidenceKind kind,
                                                std::string source,
                                                std::int64_t timestamp_ms,
                                                std::string digest = {}) {
  Provenance p;
  p.kind = kind;
  p.source = std::move(source);
  p.timestamp_ms = timestamp_ms;
  p.digest = std::move(digest);
  return p;
}

}  // namespace runtimeregistry
