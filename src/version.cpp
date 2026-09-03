#include <runtimeregistry/version.hpp>

#include <algorithm>
#include <cctype>
#include <ostream>
#include <string>
#include <vector>
#include <vector>

namespace runtimeregistry {

namespace {

// Returns true and sets out if the whole string is a non-negative decimal
// integer with no leading zeros (except "0" itself).
bool parse_u64(std::string_view s, std::int64_t* out) {
  if (s.empty()) return false;
  std::int64_t v = 0;
  if (s.size() > 1 && s[0] == '0') return false;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
    if (v < 0) return false;  // overflow guard
  }
  *out = v;
  return true;
}

bool ident_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-';
}

bool valid_dot_identifiers(std::string_view s) {
  if (s.empty()) return false;
  std::size_t start = 0;
  while (start < s.size()) {
    std::size_t dot = s.find('.', start);
    std::size_t end = (dot == std::string_view::npos) ? s.size() : dot;
    std::string_view id = s.substr(start, end - start);
    if (id.empty()) return false;
    for (char c : id)
      if (!ident_char(c)) return false;
    bool all_digit = true;
    for (char c : id)
      if (c < '0' || c > '9') { all_digit = false; break; }
    if (all_digit && id.size() > 1 && id[0] == '0') return false;
    if (dot == std::string_view::npos) break;
    start = dot + 1;
  }
  return true;
}

}  // namespace

namespace detail {
bool parse_version_components(std::string_view text, std::int64_t* major,
                              std::int64_t* minor, std::int64_t* patch) {
  if (text.empty()) return false;
  std::string s(text);
  // split on '.'
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= s.size()) {
    std::size_t dot = s.find('.', start);
    parts.push_back(s.substr(start, dot == std::string::npos
                                        ? std::string::npos
                                        : dot - start));
    if (dot == std::string::npos) break;
    start = dot + 1;
  }
  if (parts.size() < 1 || parts.size() > 3) return false;
  std::int64_t a = 0, b = 0, c = 0;
  if (!parse_u64(parts[0], &a)) return false;
  if (parts.size() >= 2 && !parse_u64(parts[1], &b)) return false;
  if (parts.size() >= 3 && !parse_u64(parts[2], &c)) return false;
  *major = a; *minor = b; *patch = c;
  return true;
}
}  // namespace detail

std::optional<SemanticVersion> SemanticVersion::parse(std::string_view text) {
  if (text.empty()) return std::nullopt;
  SemanticVersion v;
  std::string_view rest = text;

  std::size_t plus = rest.find('+');
  if (plus != std::string_view::npos) {
    v.build = std::string(rest.substr(plus + 1));
    rest = rest.substr(0, plus);
    if (!valid_dot_identifiers(v.build)) return std::nullopt;
  }

  std::size_t dash = rest.find('-');
  if (dash != std::string_view::npos) {
    v.prerelease = std::string(rest.substr(dash + 1));
    rest = rest.substr(0, dash);
    if (!valid_dot_identifiers(v.prerelease)) return std::nullopt;
  }

  std::size_t dot1 = rest.find('.');
  if (dot1 == std::string_view::npos) return std::nullopt;
  std::size_t dot2 = rest.find('.', dot1 + 1);
  if (dot2 == std::string_view::npos) return std::nullopt;  // majors require patch
  std::string_view maj = rest.substr(0, dot1);
  std::string_view min = rest.substr(dot1 + 1, dot2 - dot1 - 1);
  std::string_view pat = rest.substr(dot2 + 1);
  if (pat.find('.') != std::string_view::npos) return std::nullopt;
  if (!parse_u64(maj, &v.major)) return std::nullopt;
  if (!parse_u64(min, &v.minor)) return std::nullopt;
  if (!pat.empty()) {
    if (!parse_u64(pat, &v.patch)) return std::nullopt;
  }
  return v;
}

int SemanticVersion::compare(const SemanticVersion& other) const noexcept {
  if (major != other.major) return major < other.major ? -1 : 1;
  if (minor != other.minor) return minor < other.minor ? -1 : 1;
  if (patch != other.patch) return patch < other.patch ? -1 : 1;

  bool a_pre = !prerelease.empty();
  bool b_pre = !other.prerelease.empty();
  if (!a_pre && !b_pre) return 0;
  if (a_pre && !b_pre) return -1;
  if (!a_pre && b_pre) return 1;

  std::size_t ia = 0, ib = 0;
  while (true) {
    std::size_t da = prerelease.find('.', ia);
    std::size_t db = other.prerelease.find('.', ib);
    std::string_view A = (da == std::string::npos)
                             ? std::string_view(prerelease).substr(ia)
                             : std::string_view(prerelease).substr(ia, da - ia);
    std::string_view B = (db == std::string::npos)
                             ? std::string_view(other.prerelease).substr(ib)
                             : std::string_view(other.prerelease).substr(ib, db - ib);
    if (A.empty() && B.empty()) return 0;
    if (A.empty()) return -1;
    if (B.empty()) return 1;
    bool A_num = true, B_num = true;
    for (char c : A) if (c < '0' || c > '9') { A_num = false; break; }
    for (char c : B) if (c < '0' || c > '9') { B_num = false; break; }
    if (A_num && B_num) {
      std::int64_t an = 0, bn = 0;
      for (char c : A) an = an * 10 + (c - '0');
      for (char c : B) bn = bn * 10 + (c - '0');
      if (an != bn) return an < bn ? -1 : 1;
    } else if (A_num != B_num) {
      return A_num ? -1 : 1;
    } else {
      if (A != B) return A < B ? -1 : 1;
    }
    std::size_t ia_next = (da == std::string::npos) ? std::string::npos : da + 1;
    std::size_t ib_next = (db == std::string::npos) ? std::string::npos : db + 1;
    if (ia_next == std::string::npos || ib_next == std::string::npos) {
      if (ia_next == std::string::npos && ib_next == std::string::npos) return 0;
      return (ia_next == std::string::npos) ? -1 : 1;
    }
    ia = ia_next;
    ib = ib_next;
  }
}

bool operator==(const SemanticVersion& a, const SemanticVersion& b) {
  return a.compare(b) == 0;
}
bool operator!=(const SemanticVersion& a, const SemanticVersion& b) {
  return a.compare(b) != 0;
}
bool operator<(const SemanticVersion& a, const SemanticVersion& b) {
  return a.compare(b) < 0;
}
bool operator<=(const SemanticVersion& a, const SemanticVersion& b) {
  return a.compare(b) <= 0;
}
bool operator>(const SemanticVersion& a, const SemanticVersion& b) {
  return a.compare(b) > 0;
}
bool operator>=(const SemanticVersion& a, const SemanticVersion& b) {
  return a.compare(b) >= 0;
}

std::string SemanticVersion::str() const {
  std::string out = std::to_string(major) + "." + std::to_string(minor) + "." +
                    std::to_string(patch);
  if (!prerelease.empty()) out += "-" + prerelease;
  if (!build.empty()) out += "+" + build;
  return out;
}

}  // namespace runtimeregistry
