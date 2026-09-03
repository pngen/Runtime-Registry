#pragma once
// Typed error/result model.

#include <stdexcept>
#include <string>

namespace runtimeregistry {

enum class ErrorKind {
  STALE_EPOCH,
  STALE_BOOT,
  STALE_REGISTRY_GENERATION,
  STALE_SERVICE_GENERATION,
  STALE_INSTANCE_GENERATION,
  STALE_RUNTIME_GENERATION,
  STALE_ENDPOINT_GENERATION,
  STALE_CAPABILITY_GENERATION,
  STALE_LEASE_GENERATION,
  STALE_HEALTH,
  STALE_READINESS,
  STALE_REACHABILITY,
  STALE_INVALIDATION,
  STALE_TOMBSTONE,
  STALE_ATTEMPT,
  STALE_DISPATCH,
  DUPLICATE_CONFLICT,
  STALE_RENEWAL,
  TOMBSTONE_RESURRECTION,
  UNKNOWN_IDENTITY,
  NOT_CURRENT,
  UNREADY_ONLY,
  MALFORMED,
  INSUFFICIENT_EVIDENCE,
  INCOMPATIBLE,
  RESOURCE_BOUND,
  INTERNAL,
};

class RegistryError : public std::runtime_error {
 public:
  RegistryError(ErrorKind kind, std::string message)
      : std::runtime_error(std::move(message)), kind_(kind) {}
  [[nodiscard]] ErrorKind kind() const noexcept { return kind_; }

 private:
  ErrorKind kind_;
};

}  // namespace runtimeregistry
