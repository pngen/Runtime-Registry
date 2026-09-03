#pragma once
// First-class typed capability model. Core capabilities are strongly typed
// (boolean, integer, real, range, string/enum, structured). Arbitrary JSON is
// not the primary semantic model; extension metadata is supported separately.

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

enum class CapabilityValueKind {
  BOOLEAN,
  INTEGER,
  REAL,
  RANGE,
  STRING,
  STRUCTURED,
  UNKNOWN,
};
[[nodiscard]] std::string_view to_string(CapabilityValueKind k) noexcept;

// A typed capability value. Exactly one semantic shape is active.
struct CapabilityValue {
  CapabilityValueKind kind{CapabilityValueKind::UNKNOWN};

  bool boolean{false};
  std::int64_t integer{0};
  double real{0.0};
  std::string string;         // enum-like / textual values
  bool has_min{false};
  bool has_max{false};
  double range_min{0.0};
  double range_max{0.0};

  // Structured typed value: ordered key/value pairs with typed values.
  std::vector<std::pair<std::string, CapabilityValue>> structured;

  [[nodiscard]] static CapabilityValue make_bool(bool v) noexcept;
  [[nodiscard]] static CapabilityValue make_integer(std::int64_t v) noexcept;
  [[nodiscard]] static CapabilityValue make_real(double v) noexcept;
  [[nodiscard]] static CapabilityValue make_string(std::string v);
  [[nodiscard]] static CapabilityValue make_range(double min, double max) noexcept;
  [[nodiscard]] static CapabilityValue make_structured() noexcept;

  [[nodiscard]] std::string render() const;  // deterministic display

  friend bool operator==(const CapabilityValue& a, const CapabilityValue& b);
  friend bool operator!=(const CapabilityValue& a, const CapabilityValue& b);
};

struct CapabilityDescriptor {
  CapabilityId capability_id;
  CapabilityKind kind{CapabilityKind::UNKNOWN};
  CapabilityGeneration generation;
  SemanticVersion version;
  CapabilityValue value;             // typed value
  std::vector<std::string> attributes;   // optional extension metadata
  std::vector<std::string> constraints;  // optional text constraints
  Provenance provenance;
  Freshness freshness{Freshness::UNKNOWN};
  AuthorityGeneration authority_generation;

  friend bool operator==(const CapabilityDescriptor& a,
                         const CapabilityDescriptor& b) = default;
};

}  // namespace runtimeregistry
