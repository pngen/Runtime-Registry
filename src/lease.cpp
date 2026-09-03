// Lease/liveness management. Lease state transitions are guarded and stale
// renewals are rejected; see registry.cpp for the implementation.
#include <runtimeregistry/registry.hpp>
