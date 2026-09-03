# Runtime Registry

Runtime Registry is a vendor-neutral C++20 runtime discovery and authority layer for
heterogeneous AI infrastructure. It owns one systems boundary:

> What runtime services, nodes, devices, backends, protocols, and capabilities exist
> right now, which instances are current and healthy, what versions and contracts they
> expose, and which registry state is authoritative enough to drive discovery and binding.

It is the canonical runtime/service/capability discovery and authority layer, not a
scattered set of environment variables, static config files, ad hoc service maps, stale
process lists, or hard-coded backend assumptions.

## The systems question

Distributed AI infrastructure needs a single place to answer precisely:

- Which services and runtimes exist?
- Which process incarnation owns each instance right now?
- Which endpoints are reachable?
- Which protocols and versions are compatible?
- Which capabilities are current (and measured, not inferred)?
- Which instances are healthy and ready?
- What became stale or invalid after a failure or restart?
- Why was one candidate selected over another?
- Which registry result is authoritative enough to bind to now?

## Boundary and distinctions

Runtime Registry deliberately does not become something else:

- Not Distributed Cache Directory. It does not own the location or semantics of reusable state.
- Not State Index. It does not track the ownership or staleness of shared state values.
- Not Resource Broker. It does not allocate resources or schedule work.
- Not a DNS / Consul / etcd clone. It is a registry of typed runtime/service/capability records.

Its job is discovery and authority, not resource allocation or reusable-state location.

## Core model

Runtime Registry models the reality of modern accelerator systems with explicit, strongly
typed records:

- ServiceDescriptor - a service contract (id, kind, name, generation, owner, lifecycle, version, API, ABI).
- ServiceInstance - a live incarnation (instance id, service id, generation, node, worker, boot, endpoints, protocols, capabilities, health, readiness, freshness, reachability, lease, lifecycle).
- RuntimeDescriptor - a runtime implementation/environment (kind, name, family, version, API/ABI, build, compiler, protocols, capabilities, backends, architecture constraints).
- EndpointDescriptor - an endpoint (id, service instance, generation, protocol, locator, transport, port, health, reachability, freshness, authority).
- ProtocolDescriptor - a protocol contract with structured versions and a compatibility range.
- CapabilityDescriptor - a first-class typed capability (id, kind, generation, version, typed value, freshness, authority).
- BackendDescriptor, DeviceDescriptor, NodeDescriptor, LeaseDescriptor, TombstoneRecord, InvalidationRecord, CompatibilityRecord, ServiceDependency.

Identity is strong and non-interchangeable. ServiceId, ServiceInstanceId, RuntimeId,
RuntimeInstanceId, NodeId, WorkerId, WorkerBootId, ProcessId, EndpointId, ProtocolId,
BackendId, DeviceId, CapabilityId, LeaseId and many more are distinct typed values that
never implicitly convert to their underlying integer or to each other. Generation types
(ServiceGeneration, ServiceInstanceGeneration, RuntimeGeneration, EndpointGeneration,
CapabilityGeneration, LeaseGeneration, CoordinatorEpoch, AuthorityGeneration and so on)
support explicit comparison.

## Authority: incarnation-scoped

Authority is scoped to a process incarnation. Each logical worker has a stable WorkerId,
a fresh WorkerBootId (a unique, monotonic process incarnation), and explicit liveness.
After a worker restarts it gets a new WorkerBootId; the old boot becomes stale and revoked.
A larger generation from an old boot never fences a fresh incarnation. Every mutation
carries a full authority envelope (epoch, boot, registry/record/service/instance/runtime/
endpoint/capability/lease/attempt/dispatch generations); a stale envelope is rejected before
any state change.

## Lifecycle

Instance lifecycle is a guarded state machine: REGISTERING, AVAILABLE, DEGRADED, DRAINING,
UNREADY, STALE, UNREACHABLE, INVALIDATED, SUPERSEDED, TOMBSTONED, RETIRED, FAILED.

An instance is not AVAILABLE merely because it registered once. Availability requires
current authority, a valid process incarnation, acceptable freshness, a valid lease,
reachable required endpoints, present and current required capabilities, acceptable
readiness, acceptable health, and a compatible protocol/API contract.

## Health, readiness, freshness, reachability

These are independent models:

- Health: HEALTHY, DEGRADED, UNHEALTHY, UNAVAILABLE, UNKNOWN.
- Readiness: READY, PARTIALLY_READY, NOT_READY, DRAINING, REVALIDATION_REQUIRED, UNKNOWN.
- Freshness: CURRENT, STALE, REVALIDATION_REQUIRED, UNKNOWN.
- Reachability: REACHABLE, DEGRADED, UNREACHABLE, REVALIDATION_REQUIRED, UNKNOWN.

A healthy service can be unready. A ready service with stale authority is never AVAILABLE.
UNKNOWN readiness never becomes READY, UNKNOWN reachability never becomes REACHABLE, and
UNKNOWN capability never satisfies a required capability.

## Versions and protocol negotiation

Versions are structured (SemanticVersion, ApiVersion, AbiVersion, ProtocolVersion) and never
compared as strings. They support exact, minimum, compatible-major, and bounded-range checks;
malformed versions are rejected. 1.9 is less than 1.10, and a different major is incompatible
when major compatibility is required. Protocol negotiation is deterministic and never silently
promotes UNKNOWN; outcomes are EXACT, COMPATIBLE, DOWNGRADE_ALLOWED, UPGRADE_REQUIRED,
INCOMPATIBLE, INSUFFICIENT_EVIDENCE, UNKNOWN.

## Capabilities

Capabilities are first-class and typed (boolean, integer, real, range, string/enum, structured).
Arbitrary JSON is not the primary semantic model. Every capability carries provenance
(MEASURED, REPORTED, DERIVED, SYNTHETIC, UNKNOWN), freshness, and authority. Known kinds include
CUDA_AVAILABLE, CUDA_COMPUTE_CAPABILITY, CUDA_ARCHITECTURE, CUDA_MEMORY, LOCAL_FILESYSTEM,
TCP_TRANSPORT, FRAMED_PROTOCOL, MULTIPROCESS, PERSISTENCE, HEALTH_REPORTING, GENERIC_FEATURE and more.
Unknown device facts remain UNKNOWN; no unsupported interconnect, storage, or network
capability is ever claimed without proof.

## Leases and liveness

Leases support acquire, renew, expire, revoke, and recover-as-REVALIDATION_REQUIRED.
Liveness is not wall-clock only: session/connection/boot-based liveness is representable.
A stale boot can never renew a current lease.

## Discovery and deterministic selection

RegistryQuery supports service kind, exact service id, runtime kind/family, minimum API
version, ABI/protocol requirements, required capabilities, backend/architecture/device
requirements, required health/readiness/freshness/reachability, preferred node/runtime,
version policy, current-only, and max-result count. Hard filters eliminate candidates
first; remaining candidates are ranked by named factors (exact service id, exact
protocol/API, capability completeness, readiness, health, locality, runtime preference,
version recency, endpoint reachability, freshness, policy preference) - not one opaque
master score. Tie-breaking is deterministic, and every query returns an explanation.

## Supersession, invalidation, tombstones

A new current generation supersedes older current state. Historical entries are preserved
(as configured) but never enter a current lookup accidentally. Invalidation can target any
identity; stale invalidation never invalidates fresh state. Tombstones carry a target
identity, a generation floor, the coordinator epoch, worker boot id, and reason; a stale
service or process cannot republish an instance covered by a current tombstone, so
resurrection is prevented.

## Distributed authority: coordinator and workers

Runtime Registry uses a coordinator + worker/source model. Workers register runtimes,
services, endpoints, capabilities, update health/readiness, renew leases, invalidate,
tombstone, and deregister - over a bounded, versioned, CRC-protected framed TCP protocol
(runtime-registry-coordinator, runtime-registry-worker). The coordinator rejects stale
mutations before any state change. The protocol rejects bad magic, unsupported version,
oversized payload, truncation, checksum mismatch, invalid enum, malformed version,
malformed generation, malformed endpoint/capability, conflicting duplicate identity, and
trailing garbage. Writes are serialized per connection; no global registry lock is held
during blocking network I/O.

## Persistence and recovery

Persistence is a deterministic, versioned binary format: magic, version, bounded counts,
canonical source records, a CRC-32, and a SHA-256 semantic digest. Canonical records are
persisted (not hash-bucket layouts); indexes are rebuilt deterministically on recovery.
Atomic writes use temp, flush, close, rename.

On recovery, live WorkerBootId authority is cleared, process-local and in-process endpoints
become REVALIDATION_REQUIRED, active TCP reachability requires revalidation, dynamic
health/readiness may require revalidation, leases do not silently remain live,
CUDA/runtime/device observations requiring physical freshness become
REVALIDATION_REQUIRED, logical version/capability history remains, and tombstones remain
authoritative. The format rejects bad magic/version, truncation, checksum mismatch,
semantic-digest mismatch, impossible counts, duplicate identity, generation regression,
malformed version/endpoint/capability/tombstone, and trailing garbage.

## Real local proof

Runtime Registry is validated with real local components: a real registry coordinator, a
real worker process, a real loopback TCP endpoint, a local filesystem back end descriptor,
a native C++ runtime descriptor, and (when enabled) a real CUDA runtime/device capability
advertisement. It proves runtime registration, service registration, endpoint
registration, capability advertisement, exact lookup, capability-aware lookup,
protocol/version filtering, health/readiness/reachability transitions, lease
expiry/revalidation, supersession, invalidation, tombstone, historical query, and
persistence/recovery.

## CUDA capability proof (real RTX 5090)

When built with RUNTIMEREGISTRY_ENABLE_CUDA_PROOF=ON, the CUDA proof drives a real
device: enumerate the CUDA device (RTX 5090, compute capability 12.0), record
driver/runtime/device facts, register a CUDA RuntimeInstance and a Device, advertise real
measured capabilities (CUDA_AVAILABLE, CUDA_COMPUTE_CAPABILITY, CUDA_ARCHITECTURE,
CUDA_MEMORY) with MEASURED provenance, allocate a deterministic buffer, H2D, run a real
kernel, synchronize, D2H, verify CPU-reference parity, register a service requiring CUDA and
select the compatible instance via a capability-aware query, show that a stale
capability-generation replay is rejected, refresh the device generation and confirm a
fresh query succeeds, free the buffer and verify clean shutdown. The proof distinguishes
MEASURED, REPORTED, DERIVED, SYNTHETIC, and UNKNOWN evidence and never claims unsupported
transports or interconnects.

## Synthetic distributed scenarios

Where physical multi-node infrastructure is unavailable, deterministic synthetic
scenarios (provenance SYNTHETIC) cover: two equivalent services on different nodes, exact
version vs older version, compatible protocol downgrade, incompatible protocol major,
missing required capability, healthy-but-unready, ready-but-unreachable, stale/fresh lease,
worker restart, service/runtime/endpoint/device/capability generation rollover, stale
health/readiness updates, tombstone resurrection prevention, exact-far vs compatible-local,
deterministic tie-break, unknown capability/readiness/reachability, and policy-preference
changes.

## Command line

Build runtime-registry for a CLI exposing runtime-register, service-register,
endpoint-register, capability-register, show, discover, capabilities, health, readiness,
reachability, lease-renew, invalidate, tombstone, history, explain, simulate, save, recover,
and benchmark command styles, and printing service/instance/runtime/node/boot ids,
endpoints, protocol/version, API/ABI version, capabilities, health, readiness, freshness,
reachability, lease state, authority, provenance, query outcome, selected candidate, and
rejection reasons.

## Building

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Key CMake options:

- RUNTIMEREGISTRY_BUILD_TESTS (ON) - build the ctest suites.
- RUNTIMEREGISTRY_BUILD_EXAMPLES (ON) - build the runnable examples.
- RUNTIMEREGISTRY_BUILD_BENCHMARKS (ON) - build the completed-work benchmarks.
- RUNTIMEREGISTRY_ENABLE_SYNTHETIC_DISTRIBUTED (ON) - build the coordinator/worker and synthetic distributed proof.
- RUNTIMEREGISTRY_ENABLE_CUDA_PROOF (OFF) - build the real CUDA capability proof. CUDA is optional and defaults to OFF.

The build is clean under MSVC 19.44 with /W4 /WX (warnings-as-errors).

## Examples

The examples directory contains runnable programs using the real public library API:
service identity, runtime registration, endpoint registration, capability advertisement,
exact discovery, version filtering, protocol negotiation, capability query,
health/readiness, reachability, lease liveness, supersession/tombstone, and
persistence/recovery.

## Benchmarks

The benchmark_core program measures completed-work throughput (registration,
service-kind lookup, capability lookup, version filtering, protocol encode/decode,
persistence serialize/recover) across registry sizes of 1,000 and 10,000 instances. A
100,000-instance registry is not populated on this host: per-registration index rebuild is
O(n^2) and candidate ranking is O(n) for the current query path, so 100k is reported as not
practical here rather than benchmarked with an empty loop.

## Downstream package consumption

Runtime Registry installs a CMake package. An independent consumer can simply do:

```cmake
find_package(RuntimeRegistry CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE RuntimeRegistry::runtimeregistry)
```

The installed package exports RuntimeRegistry::runtimeregistry, headers, and a version
config with no build-tree path leakage. The tools/downstream consumer project builds and
runs against a clean install.

## Limitations

State once, precisely:

- Real local / process / loopback-TCP / CUDA capabilities are physically validated where the proofs pass.
- Remote multi-node services are synthetic where physical infrastructure is unavailable.
- Runtime Registry discovers and governs runtime/service capabilities; it does not allocate resources or own reusable-state location semantics.
- Process-local endpoints and dynamic runtime/device observations require revalidation after a coordinator restart.
- No unsupported interconnect/storage/network capability is ever claimed.
- Unknown facts remain UNKNOWN.

No telemetry is transmitted. All observations, logs, benchmarks, and persistence are local
to the host that runs it.

## License
Apache License 2.0. Copyright 2026 Summon Software Labs. No telemetry transmission.