#pragma once
// Deterministic versioned binary persistence. Canonical source records are
// persisted, not hash-bucket layouts. Indexes are rebuilt deterministically on
// recovery. Format: magic | version | counts | records | CRC-32 | SHA-256.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace runtimeregistry {

class Registry;

struct PersistenceResult {
  bool ok{false};
  std::string error;
  std::string semantic_digest;   // of loaded logical state
};

// Serialize the registry to a deterministic byte stream.
void serialize_registry(const Registry& reg, std::vector<std::uint8_t>& out);

// Load and rebuild a registry from a byte stream. Throws RegistryError on
// corrupt/truncated/invalid input.
void load_registry(Registry& reg, const std::uint8_t* data, std::size_t size);

[[nodiscard]] std::string registry_semantic_digest(const Registry& reg);

}  // namespace runtimeregistry
