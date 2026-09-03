#pragma once
// Structured version model. Versions are never compared as strings.
//
// SemanticVersion supports full semver ordering including prerelease rules.
// ApiVersion / AbiVersion / ProtocolVersion support major.minor(.patch) with
// exact, minimum, compatible-major, and bounded-range checks.

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace runtimeregistry {

// Full semantic version. major.minor.patch[-prerelease][+build].
struct SemanticVersion {
  std::int64_t major{0};
  std::int64_t minor{0};
  std::int64_t patch{0};
  std::string prerelease;  // optional "-..." text without leading '-'
  std::string build;       // optional "+..." text without leading '+'

  static std::optional<SemanticVersion> parse(std::string_view text);
  [[nodiscard]] int compare(const SemanticVersion& other) const noexcept;

  friend bool operator==(const SemanticVersion& a, const SemanticVersion& b);
  friend bool operator!=(const SemanticVersion& a, const SemanticVersion& b);
  friend bool operator<(const SemanticVersion& a, const SemanticVersion& b);
  friend bool operator<=(const SemanticVersion& a, const SemanticVersion& b);
  friend bool operator>(const SemanticVersion& a, const SemanticVersion& b);
  friend bool operator>=(const SemanticVersion& a, const SemanticVersion& b);

  [[nodiscard]] std::string str() const;
};

// Non-interchangeable structured version used for API / ABI / protocol
// contracts. major.minor(.patch). Not string-compared.
template <typename Tag>
class StructuredVersion {
 public:
  constexpr StructuredVersion() = default;
  constexpr StructuredVersion(std::int64_t major, std::int64_t minor,
                              std::int64_t patch = 0)
      : major_(major), minor_(minor), patch_(patch) {}

  [[nodiscard]] constexpr std::int64_t major() const noexcept { return major_; }
  [[nodiscard]] constexpr std::int64_t minor() const noexcept { return minor_; }
  [[nodiscard]] constexpr std::int64_t patch() const noexcept { return patch_; }

  void set_major(std::int64_t m) noexcept { major_ = m; }
  void set_minor(std::int64_t m) noexcept { minor_ = m; }
  void set_patch(std::int64_t p) noexcept { patch_ = p; }

  static std::optional<StructuredVersion> parse(std::string_view text);

  [[nodiscard]] int compare(const StructuredVersion& other) const noexcept;

  friend bool operator==(const StructuredVersion& a, const StructuredVersion& b) {
    return a.compare(b) == 0;
  }
  friend bool operator!=(const StructuredVersion& a, const StructuredVersion& b) {
    return a.compare(b) != 0;
  }
  friend bool operator<(const StructuredVersion& a, const StructuredVersion& b) {
    return a.compare(b) < 0;
  }
  friend bool operator<=(const StructuredVersion& a, const StructuredVersion& b) {
    return a.compare(b) <= 0;
  }
  friend bool operator>(const StructuredVersion& a, const StructuredVersion& b) {
    return a.compare(b) > 0;
  }
  friend bool operator>=(const StructuredVersion& a, const StructuredVersion& b) {
    return a.compare(b) >= 0;
  }

  [[nodiscard]] std::string str() const {
    std::string out =
        std::to_string(major_) + "." + std::to_string(minor_);
    if (patch_ != 0) out += "." + std::to_string(patch_);
    return out;
  }

  friend inline std::ostream& operator<<(std::ostream& os,
                                         const StructuredVersion& v) {
    return os << v.str();
  }

  [[nodiscard]] bool is_exact(const StructuredVersion& other) const noexcept {
    return compare(other) == 0;
  }
  [[nodiscard]] bool satisfies_min(const StructuredVersion& minimum) const noexcept {
    return compare(minimum) >= 0;
  }
  [[nodiscard]] bool major_compatible(const StructuredVersion& other) const noexcept {
    return major_ == other.major_;
  }
  [[nodiscard]] bool within_range(const StructuredVersion& low,
                                  const StructuredVersion& high) const noexcept {
    return compare(low) >= 0 && compare(high) <= 0;
  }

 private:
  std::int64_t major_{0};
  std::int64_t minor_{0};
  std::int64_t patch_{0};
};

namespace detail {
// Parses "a", "a.b", or "a.b.c" into major/minor/patch. Returns false on
// malformed input (empty, non-digits, too many components, trailing junk).
[[nodiscard]] bool parse_version_components(std::string_view text,
                                            std::int64_t* major,
                                            std::int64_t* minor,
                                            std::int64_t* patch);
}  // namespace detail

template <typename Tag>
std::optional<StructuredVersion<Tag>> StructuredVersion<Tag>::parse(
    std::string_view text) {
  std::int64_t a = 0, b = 0, c = 0;
  if (!detail::parse_version_components(text, &a, &b, &c)) return std::nullopt;
  return StructuredVersion<Tag>(a, b, c);
}

template <typename Tag>
int StructuredVersion<Tag>::compare(const StructuredVersion& other) const noexcept {
  if (major_ != other.major_) return major_ < other.major_ ? -1 : 1;
  if (minor_ != other.minor_) return minor_ < other.minor_ ? -1 : 1;
  if (patch_ != other.patch_) return patch_ < other.patch_ ? -1 : 1;
  return 0;
}

struct ApiTag;
struct AbiTag;
struct ProtocolTag;

using ApiVersion = StructuredVersion<ApiTag>;
using AbiVersion = StructuredVersion<AbiTag>;
using ProtocolVersion = StructuredVersion<ProtocolTag>;

}  // namespace runtimeregistry
