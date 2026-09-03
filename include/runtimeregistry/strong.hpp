#pragma once
// Runtime Registry core strong-type primitives.
// Identities and generations are distinct, non-interchangeable, strongly
// typed wrappers. They never implicitly convert to their underlying integer
// or to each other.

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <ostream>
#include <type_traits>

namespace runtimeregistry {

// Opaque, ordered, strongly-typed integer-backed identity.
template <typename Tag, typename Rep = std::uint64_t>
class Id {
 public:
  using rep = Rep;
  constexpr Id() = default;
  constexpr explicit Id(Rep value) noexcept : value_(value) {}

  [[nodiscard]] constexpr Rep value() const noexcept { return value_; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return value_ != Rep{0};
  }

  constexpr auto operator<=>(const Id&) const = default;

  friend std::ostream& operator<<(std::ostream& os, const Id& id) {
    return os << "Id(" << id.value_ << ")";
  }

 private:
  Rep value_{};
};

// Monotonic, ordered, strongly-typed generation. Generations are compared
// explicitly and never implicitly converted.
template <typename Tag, typename Rep = std::uint64_t>
class Gen {
 public:
  using rep = Rep;
  constexpr Gen() = default;
  constexpr explicit Gen(Rep value) noexcept : value_(value) {}

  [[nodiscard]] constexpr Rep value() const noexcept { return value_; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return value_ != Rep{0};
  }

  constexpr Gen& operator++() noexcept {
    ++value_;
    return *this;
  }
  constexpr Gen operator++(int) noexcept {
    Gen tmp = *this;
    ++value_;
    return tmp;
  }
  constexpr Gen& operator+=(Rep delta) noexcept {
    value_ += delta;
    return *this;
  }

  constexpr auto operator<=>(const Gen&) const = default;

  friend std::ostream& operator<<(std::ostream& os, const Gen& g) {
    return os << "Gen(" << g.value_ << ")";
  }

 private:
  Rep value_{};
};

}  // namespace runtimeregistry

// std::hash specializations for the strong types.
namespace std {

template <typename Tag, typename Rep>
struct hash<::runtimeregistry::Id<Tag, Rep>> {
  size_t operator()(const ::runtimeregistry::Id<Tag, Rep>& id) const noexcept {
    ::std::hash<Rep> h;
    return h(id.value());
  }
};

template <typename Tag, typename Rep>
struct hash<::runtimeregistry::Gen<Tag, Rep>> {
  size_t operator()(const ::runtimeregistry::Gen<Tag, Rep>& g) const noexcept {
    ::std::hash<Rep> h;
    return h(g.value());
  }
};

}  // namespace std
