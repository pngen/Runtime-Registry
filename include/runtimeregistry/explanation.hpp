#pragma once
// Explanation renderers. Free functions constructing human/digest-stable text.

#include <runtimeregistry/enums.hpp>
#include <runtimeregistry/identity.hpp>
#include <string>

namespace runtimeregistry {

[[nodiscard]] std::string render_health(Health h) noexcept;
[[nodiscard]] std::string render_readiness(Readiness r) noexcept;

}  // namespace runtimeregistry
