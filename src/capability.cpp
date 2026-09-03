#include <runtimeregistry/capability.hpp>

#include <iomanip>
#include <ostream>
#include <sstream>

namespace runtimeregistry {

std::string_view to_string(CapabilityValueKind k) noexcept {
  switch (k) {
    case CapabilityValueKind::BOOLEAN: return "BOOLEAN";
    case CapabilityValueKind::INTEGER: return "INTEGER";
    case CapabilityValueKind::REAL: return "REAL";
    case CapabilityValueKind::RANGE: return "RANGE";
    case CapabilityValueKind::STRING: return "STRING";
    case CapabilityValueKind::STRUCTURED: return "STRUCTURED";
    case CapabilityValueKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

CapabilityValue CapabilityValue::make_bool(bool v) noexcept {
  CapabilityValue cv;
  cv.kind = CapabilityValueKind::BOOLEAN;
  cv.boolean = v;
  return cv;
}
CapabilityValue CapabilityValue::make_integer(std::int64_t v) noexcept {
  CapabilityValue cv;
  cv.kind = CapabilityValueKind::INTEGER;
  cv.integer = v;
  return cv;
}
CapabilityValue CapabilityValue::make_real(double v) noexcept {
  CapabilityValue cv;
  cv.kind = CapabilityValueKind::REAL;
  cv.real = v;
  return cv;
}
CapabilityValue CapabilityValue::make_string(std::string v) {
  CapabilityValue cv;
  cv.kind = CapabilityValueKind::STRING;
  cv.string = std::move(v);
  return cv;
}
CapabilityValue CapabilityValue::make_range(double min, double max) noexcept {
  CapabilityValue cv;
  cv.kind = CapabilityValueKind::RANGE;
  cv.has_min = true;
  cv.has_max = true;
  cv.range_min = min;
  cv.range_max = max;
  return cv;
}
CapabilityValue CapabilityValue::make_structured() noexcept {
  CapabilityValue cv;
  cv.kind = CapabilityValueKind::STRUCTURED;
  return cv;
}

// Deterministic, human/digest-stable rendering.
std::string CapabilityValue::render() const {
  std::ostringstream os;
  switch (kind) {
    case CapabilityValueKind::BOOLEAN:
      os << (boolean ? "true" : "false");
      break;
    case CapabilityValueKind::INTEGER:
      os << integer;
      break;
    case CapabilityValueKind::REAL:
      os << std::setprecision(12) << real;
      break;
    case CapabilityValueKind::RANGE:
      os << "[";
      if (has_min) os << range_min;
      os << ",";
      if (has_max) os << range_max;
      os << "]";
      break;
    case CapabilityValueKind::STRING:
      os << string;
      break;
    case CapabilityValueKind::STRUCTURED:
      os << "{";
      for (std::size_t i = 0; i < structured.size(); ++i) {
        if (i) os << ",";
        os << structured[i].first << "=" << structured[i].second.render();
      }
      os << "}";
      break;
    case CapabilityValueKind::UNKNOWN:
      os << "UNKNOWN";
      break;
  }
  return os.str();
}

bool operator==(const CapabilityValue& a, const CapabilityValue& b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case CapabilityValueKind::BOOLEAN: return a.boolean == b.boolean;
    case CapabilityValueKind::INTEGER: return a.integer == b.integer;
    case CapabilityValueKind::REAL: return a.real == b.real;
    case CapabilityValueKind::RANGE:
      return a.has_min == b.has_min && a.has_max == b.has_max &&
             a.range_min == b.range_min && a.range_max == b.range_max;
    case CapabilityValueKind::STRING: return a.string == b.string;
    case CapabilityValueKind::STRUCTURED: return a.structured == b.structured;
    case CapabilityValueKind::UNKNOWN: return true;
  }
  return false;
}
bool operator!=(const CapabilityValue& a, const CapabilityValue& b) {
  return !(a == b);
}

}  // namespace runtimeregistry
