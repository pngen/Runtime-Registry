#include <runtimeregistry/provenance.hpp>

namespace runtimeregistry {

std::string_view to_string(EvidenceKind k) noexcept {
  switch (k) {
    case EvidenceKind::MEASURED: return "MEASURED";
    case EvidenceKind::REPORTED: return "REPORTED";
    case EvidenceKind::DERIVED: return "DERIVED";
    case EvidenceKind::SYNTHETIC: return "SYNTHETIC";
    case EvidenceKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

EvidenceKind evidence_kind_from_string(std::string_view s) noexcept {
  if (s == "MEASURED") return EvidenceKind::MEASURED;
  if (s == "REPORTED") return EvidenceKind::REPORTED;
  if (s == "DERIVED") return EvidenceKind::DERIVED;
  if (s == "SYNTHETIC") return EvidenceKind::SYNTHETIC;
  return EvidenceKind::UNKNOWN;
}

}  // namespace runtimeregistry
