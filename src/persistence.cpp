#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/persistence.hpp>
#include <runtimeregistry/crc32.hpp>
#include <runtimeregistry/sha256.hpp>
#include <runtimeregistry/error.hpp>

#include <algorithm>
#include <cstring>
#include <string>

namespace runtimeregistry {

namespace {

constexpr std::uint32_t kPersistMagic = 0x52525031u;
constexpr std::uint8_t kPersistVersion = 1;
constexpr std::uint32_t kMaxRecordsPerMap = 10000000u;

void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
  b.push_back(static_cast<std::uint8_t>(v & 0xFF)); b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF)); b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF)); b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}
void put_u64(std::vector<std::uint8_t>& b, std::uint64_t v) { put_u32(b, static_cast<std::uint32_t>(v & 0xFFFFFFFF)); put_u32(b, static_cast<std::uint32_t>(v >> 32)); }
void put_u8(std::vector<std::uint8_t>& b, std::uint8_t v) { b.push_back(v); }
void put_str(std::vector<std::uint8_t>& b, const std::string& s) { put_u32(b, static_cast<std::uint32_t>(s.size())); b.insert(b.end(), s.begin(), s.end()); }
void put_f64(std::vector<std::uint8_t>& b, double d) { std::uint64_t bits; std::memcpy(&bits, &d, 8); put_u64(b, bits); }

struct R {
  const std::uint8_t* p; std::size_t n; std::size_t pos = 0; bool ok = true;
  R(const std::uint8_t* data, std::size_t size) : p(data), n(size) {}
  std::uint8_t u8() { if (pos + 1 > n) { ok = false; return 0; } return p[pos++]; }
  std::uint32_t u32() { if (pos + 4 > n) { ok = false; return 0; } std::uint32_t v = static_cast<std::uint32_t>(p[pos]) | (static_cast<std::uint32_t>(p[pos+1]) << 8) | (static_cast<std::uint32_t>(p[pos+2]) << 16) | (static_cast<std::uint32_t>(p[pos+3]) << 24); pos += 4; return v; }
  std::uint64_t u64() { std::uint64_t lo = u32(); std::uint64_t hi = u32(); return lo | (hi << 32); }
  double f64() { std::uint64_t bits = u64(); double d; std::memcpy(&d, &bits, 8); return d; }
  std::string str() { std::uint32_t len = u32(); if (!ok || pos + len > n || len > (1u << 24)) { ok = false; return {}; } std::string s(reinterpret_cast<const char*>(p + pos), len); pos += len; return s; }
};

void put_version(std::vector<std::uint8_t>& b, const SemanticVersion& v) { put_u64(b, static_cast<std::uint64_t>(v.major)); put_u64(b, static_cast<std::uint64_t>(v.minor)); put_u64(b, static_cast<std::uint64_t>(v.patch)); put_str(b, v.prerelease); put_str(b, v.build); }
SemanticVersion get_version(R& r) { SemanticVersion v; v.major = static_cast<std::int64_t>(r.u64()); v.minor = static_cast<std::int64_t>(r.u64()); v.patch = static_cast<std::int64_t>(r.u64()); v.prerelease = r.str(); v.build = r.str(); return v; }

template <typename Ver> void put_sv(std::vector<std::uint8_t>& b, const Ver& v) { put_u64(b, static_cast<std::uint64_t>(v.major())); put_u64(b, static_cast<std::uint64_t>(v.minor())); put_u64(b, static_cast<std::uint64_t>(v.patch())); }
template <typename Ver> Ver get_sv(R& r) { std::int64_t a = static_cast<std::int64_t>(r.u64()); std::int64_t b2 = static_cast<std::int64_t>(r.u64()); std::int64_t c = static_cast<std::int64_t>(r.u64()); return Ver(a, b2, c); }

void put_prov(std::vector<std::uint8_t>& b, const Provenance& p) { put_u8(b, static_cast<std::uint8_t>(p.kind)); put_str(b, p.source); put_u64(b, static_cast<std::uint64_t>(p.timestamp_ms)); put_str(b, p.digest); }
Provenance get_prov(R& r) { Provenance p; p.kind = static_cast<EvidenceKind>(r.u8()); p.source = r.str(); p.timestamp_ms = static_cast<std::int64_t>(r.u64()); p.digest = r.str(); return p; }

void put_capval(std::vector<std::uint8_t>& b, const CapabilityValue& cv) {
  put_u8(b, static_cast<std::uint8_t>(cv.kind)); put_u8(b, cv.boolean ? 1 : 0); put_u64(b, static_cast<std::uint64_t>(cv.integer)); put_f64(b, cv.real); put_u8(b, cv.has_min ? 1 : 0); put_u8(b, cv.has_max ? 1 : 0); put_f64(b, cv.range_min); put_f64(b, cv.range_max); put_str(b, cv.string);
  put_u32(b, static_cast<std::uint32_t>(cv.structured.size()));
  for (size_t i = 0; i < cv.structured.size(); ++i) { put_str(b, cv.structured[i].first); put_capval(b, cv.structured[i].second); }
}
CapabilityValue get_capval(R& r) {
  CapabilityValue cv; cv.kind = static_cast<CapabilityValueKind>(r.u8()); cv.boolean = r.u8() != 0; cv.integer = static_cast<std::int64_t>(r.u64()); cv.real = r.f64(); cv.has_min = r.u8() != 0; cv.has_max = r.u8() != 0; cv.range_min = r.f64(); cv.range_max = r.f64(); cv.string = r.str();
  std::uint32_t len = r.u32(); if (len > 4096) { r.ok = false; return cv; }
  for (std::uint32_t i = 0; i < len; ++i) { std::string k = r.str(); CapabilityValue v = get_capval(r); cv.structured.emplace_back(std::move(k), std::move(v)); }
  return cv;
}

void put_id(std::vector<std::uint8_t>& b, std::uint64_t v) { put_u64(b, v); }
std::uint64_t get_id(R& r) { return r.u64(); }


template <typename VecId> void put_idvec(std::vector<std::uint8_t>& b, const VecId& v) { put_u32(b, static_cast<std::uint32_t>(v.size())); for (auto e : v) put_id(b, static_cast<std::uint64_t>(e.value())); }
template <typename VecId> VecId get_idvec(R& r, std::uint32_t max) {
  VecId out; std::uint32_t n = r.u32(); if (n > max) { r.ok = false; return out; }
  for (std::uint32_t i = 0; i < n; ++i) { typename VecId::value_type id; id = typename VecId::value_type(static_cast<std::uint64_t>(r.u64())); out.push_back(id); }
  return out;
}

void put_servicedesc(std::vector<std::uint8_t>& b, const ServiceDescriptor& s) {
  put_id(b, s.service_id.value()); put_u8(b, static_cast<std::uint8_t>(s.kind)); put_str(b, s.name); put_id(b, s.generation.value()); put_id(b, s.owner.value()); put_u8(b, static_cast<std::uint8_t>(s.lifecycle));
  put_version(b, s.version); put_sv(b, s.api_version); put_sv(b, s.abi_version);
  put_u32(b, static_cast<std::uint32_t>(s.required_capabilities.size())); for (auto c : s.required_capabilities) put_u8(b, static_cast<std::uint8_t>(c));
  put_u32(b, static_cast<std::uint32_t>(s.optional_capabilities.size())); for (auto c : s.optional_capabilities) put_u8(b, static_cast<std::uint8_t>(c));
  put_u32(b, static_cast<std::uint32_t>(s.protocol_requirements.size())); for (auto c : s.protocol_requirements) put_id(b, c.value());
  put_id(b, s.compatibility_ref.value()); put_id(b, s.policy_generation.value()); put_prov(b, s.provenance); put_str(b, s.semantic_digest);
}
ServiceDescriptor get_servicedesc(R& r) {
  ServiceDescriptor s; s.service_id = ServiceId(get_id(r)); s.kind = static_cast<ServiceKind>(r.u8()); s.name = r.str(); s.generation = ServiceGeneration(get_id(r)); s.owner = OwnerId(get_id(r)); s.lifecycle = static_cast<Lifecycle>(r.u8());
  s.version = get_version(r); s.api_version = get_sv<ApiVersion>(r); s.abi_version = get_sv<AbiVersion>(r);
  std::uint32_t nr = r.u32(); if (nr > 256) r.ok = false; for (std::uint32_t i = 0; i < nr && r.ok; ++i) s.required_capabilities.push_back(static_cast<CapabilityKind>(r.u8()));
  std::uint32_t no = r.u32(); if (no > 256) r.ok = false; for (std::uint32_t i = 0; i < no && r.ok; ++i) s.optional_capabilities.push_back(static_cast<CapabilityKind>(r.u8()));
  std::uint32_t np = r.u32(); if (np > 256) r.ok = false; for (std::uint32_t i = 0; i < np && r.ok; ++i) s.protocol_requirements.push_back(ProtocolId(get_id(r)));
  s.compatibility_ref = CompatibilityId(get_id(r)); s.policy_generation = PolicyGeneration(get_id(r)); s.provenance = get_prov(r); s.semantic_digest = r.str();
  return s;
}

void put_instance(std::vector<std::uint8_t>& b, const ServiceInstance& i) {
  put_id(b, i.instance_id.value()); put_id(b, i.service_id.value()); put_id(b, i.service_generation.value()); put_id(b, i.instance_generation.value());
  put_id(b, i.node.value()); put_id(b, i.worker.value()); put_id(b, i.boot.value()); put_id(b, i.process.value());
  put_id(b, i.runtime_id.value()); put_id(b, i.runtime_instance.value());
  put_u32(b, static_cast<std::uint32_t>(i.endpoints.size())); for (auto e : i.endpoints) put_id(b, e.value());
  put_u32(b, static_cast<std::uint32_t>(i.protocols.size())); for (auto e : i.protocols) put_id(b, e.value());
  put_u32(b, static_cast<std::uint32_t>(i.capabilities.size())); for (auto e : i.capabilities) put_id(b, e.value());
  put_u8(b, static_cast<std::uint8_t>(i.health)); put_u8(b, static_cast<std::uint8_t>(i.readiness)); put_u8(b, static_cast<std::uint8_t>(i.freshness)); put_u8(b, static_cast<std::uint8_t>(i.reachability));
  put_id(b, i.lease.value()); put_u8(b, static_cast<std::uint8_t>(i.lifecycle)); put_version(b, i.version); put_id(b, i.compatibility_ref.value()); put_id(b, i.authority_generation.value());
  put_u64(b, static_cast<std::uint64_t>(i.registered_ms)); put_u64(b, static_cast<std::uint64_t>(i.updated_ms)); put_u64(b, static_cast<std::uint64_t>(i.expires_ms));
  put_prov(b, i.provenance); put_str(b, i.semantic_digest);
}
ServiceInstance get_instance(R& r) {
  ServiceInstance i; i.instance_id = ServiceInstanceId(get_id(r)); i.service_id = ServiceId(get_id(r)); i.service_generation = ServiceGeneration(get_id(r)); i.instance_generation = ServiceInstanceGeneration(get_id(r));
  i.node = NodeId(get_id(r)); i.worker = WorkerId(get_id(r)); i.boot = WorkerBootId(get_id(r)); i.process = ProcessId(get_id(r));
  i.runtime_id = RuntimeId(get_id(r)); i.runtime_instance = RuntimeInstanceId(get_id(r));
  std::uint32_t ne = r.u32(); if (ne > 4096) r.ok = false; for (std::uint32_t k = 0; k < ne && r.ok; ++k) i.endpoints.push_back(EndpointId(get_id(r)));
  std::uint32_t np = r.u32(); if (np > 4096) r.ok = false; for (std::uint32_t k = 0; k < np && r.ok; ++k) i.protocols.push_back(ProtocolId(get_id(r)));
  std::uint32_t nc = r.u32(); if (nc > 4096) r.ok = false; for (std::uint32_t k = 0; k < nc && r.ok; ++k) i.capabilities.push_back(CapabilityId(get_id(r)));
  i.health = static_cast<Health>(r.u8()); i.readiness = static_cast<Readiness>(r.u8()); i.freshness = static_cast<Freshness>(r.u8()); i.reachability = static_cast<Reachability>(r.u8());
  i.lease = LeaseId(get_id(r)); i.lifecycle = static_cast<Lifecycle>(r.u8()); i.version = get_version(r); i.compatibility_ref = CompatibilityId(get_id(r)); i.authority_generation = AuthorityGeneration(get_id(r));
  i.registered_ms = static_cast<std::int64_t>(r.u64()); i.updated_ms = static_cast<std::int64_t>(r.u64()); i.expires_ms = static_cast<std::int64_t>(r.u64());
  i.provenance = get_prov(r); i.semantic_digest = r.str();
  return i;
}

void put_runtime(std::vector<std::uint8_t>& b, const RuntimeDescriptor& rt) {
  put_id(b, rt.runtime_id.value()); put_id(b, rt.runtime_instance.value()); put_id(b, rt.generation.value()); put_u8(b, static_cast<std::uint8_t>(rt.kind));
  put_str(b, rt.name); put_str(b, rt.family); put_version(b, rt.version); put_sv(b, rt.api_version); put_sv(b, rt.abi_version);
  put_str(b, rt.build_identity); put_str(b, rt.compiler_identity);
  put_u32(b, static_cast<std::uint32_t>(rt.supported_protocols.size())); for (auto e : rt.supported_protocols) put_id(b, e.value());
  put_u32(b, static_cast<std::uint32_t>(rt.capabilities.size())); for (auto e : rt.capabilities) put_id(b, e.value());
  put_u32(b, static_cast<std::uint32_t>(rt.backends.size())); for (auto e : rt.backends) put_id(b, e.value());
  put_str(b, rt.architecture); put_str(b, rt.device_constraint); put_prov(b, rt.provenance); put_id(b, rt.node.value()); put_id(b, rt.owner_worker.value()); put_id(b, rt.owner_boot.value());
}
RuntimeDescriptor get_runtime(R& r) {
  RuntimeDescriptor rt; rt.runtime_id = RuntimeId(get_id(r)); rt.runtime_instance = RuntimeInstanceId(get_id(r)); rt.generation = RuntimeGeneration(get_id(r)); rt.kind = static_cast<RuntimeKind>(r.u8());
  rt.name = r.str(); rt.family = r.str(); rt.version = get_version(r); rt.api_version = get_sv<ApiVersion>(r); rt.abi_version = get_sv<AbiVersion>(r);
  rt.build_identity = r.str(); rt.compiler_identity = r.str();
  std::uint32_t n1 = r.u32(); if (n1 > 4096) r.ok = false; for (std::uint32_t k = 0; k < n1 && r.ok; ++k) rt.supported_protocols.push_back(ProtocolId(get_id(r)));
  std::uint32_t n2 = r.u32(); if (n2 > 4096) r.ok = false; for (std::uint32_t k = 0; k < n2 && r.ok; ++k) rt.capabilities.push_back(CapabilityId(get_id(r)));
  std::uint32_t n3 = r.u32(); if (n3 > 4096) r.ok = false; for (std::uint32_t k = 0; k < n3 && r.ok; ++k) rt.backends.push_back(BackendId(get_id(r)));
  rt.architecture = r.str(); rt.device_constraint = r.str(); rt.provenance = get_prov(r); rt.node = NodeId(get_id(r)); rt.owner_worker = WorkerId(get_id(r)); rt.owner_boot = WorkerBootId(get_id(r));
  return rt;
}

void put_endpoint(std::vector<std::uint8_t>& b, const EndpointDescriptor& e) {
  put_id(b, e.endpoint_id.value()); put_id(b, e.service_instance.value()); put_id(b, e.generation.value()); put_id(b, e.protocol.value()); put_str(b, e.locator.text);
  put_u8(b, static_cast<std::uint8_t>(e.transport)); put_u32(b, e.port); put_u8(b, static_cast<std::uint8_t>(e.security));
  put_u8(b, static_cast<std::uint8_t>(e.health)); put_u8(b, static_cast<std::uint8_t>(e.reachability)); put_u8(b, static_cast<std::uint8_t>(e.freshness));
  put_prov(b, e.provenance); put_id(b, e.authority_generation.value());
}
EndpointDescriptor get_endpoint(R& r) {
  EndpointDescriptor e; e.endpoint_id = EndpointId(get_id(r)); e.service_instance = ServiceInstanceId(get_id(r)); e.generation = EndpointGeneration(get_id(r)); e.protocol = ProtocolId(get_id(r)); e.locator.text = r.str();
  e.transport = static_cast<TransportKind>(r.u8()); e.port = static_cast<std::uint16_t>(r.u32()); e.security = static_cast<EndpointSecurity>(r.u8());
  e.health = static_cast<Health>(r.u8()); e.reachability = static_cast<Reachability>(r.u8()); e.freshness = static_cast<Freshness>(r.u8());
  e.provenance = get_prov(r); e.authority_generation = AuthorityGeneration(get_id(r));
  return e;
}

void put_protocol(std::vector<std::uint8_t>& b, const ProtocolDescriptor& p) {
  put_id(b, p.protocol_id.value()); put_str(b, p.name); put_id(b, p.generation.value()); put_sv(b, p.version); put_sv(b, p.compatibility_min); put_str(b, p.framing);
  put_u32(b, static_cast<std::uint32_t>(p.required_capabilities.size())); for (auto c : p.required_capabilities) put_u8(b, static_cast<std::uint8_t>(c));
  put_u64(b, static_cast<std::uint64_t>(p.max_payload)); put_prov(b, p.provenance);
}
ProtocolDescriptor get_protocol(R& r) {
  ProtocolDescriptor p; p.protocol_id = ProtocolId(get_id(r)); p.name = r.str(); p.generation = ProtocolGeneration(get_id(r)); p.version = get_sv<ProtocolVersion>(r); p.compatibility_min = get_sv<ProtocolVersion>(r); p.framing = r.str();
  std::uint32_t n = r.u32(); if (n > 256) r.ok = false; for (std::uint32_t k = 0; k < n && r.ok; ++k) p.required_capabilities.push_back(static_cast<CapabilityKind>(r.u8()));
  p.max_payload = static_cast<std::int64_t>(r.u64()); p.provenance = get_prov(r);
  return p;
}

void put_backend(std::vector<std::uint8_t>& b, const BackendDescriptor& bd) {
  put_id(b, bd.backend_id.value()); put_u8(b, static_cast<std::uint8_t>(bd.kind)); put_id(b, bd.generation.value()); put_id(b, bd.runtime_binding.value());
  put_u32(b, static_cast<std::uint32_t>(bd.capabilities.size())); for (auto c : bd.capabilities) put_id(b, c.value());
  put_version(b, bd.version); put_u8(b, static_cast<std::uint8_t>(bd.health)); put_u8(b, static_cast<std::uint8_t>(bd.readiness)); put_u8(b, static_cast<std::uint8_t>(bd.freshness)); put_prov(b, bd.provenance);
}
BackendDescriptor get_backend(R& r) {
  BackendDescriptor bd; bd.backend_id = BackendId(get_id(r)); bd.kind = static_cast<BackendKind>(r.u8()); bd.generation = BackendGeneration(get_id(r)); bd.runtime_binding = RuntimeInstanceId(get_id(r));
  std::uint32_t n = r.u32(); if (n > 4096) r.ok = false; for (std::uint32_t k = 0; k < n && r.ok; ++k) bd.capabilities.push_back(CapabilityId(get_id(r)));
  bd.version = get_version(r); bd.health = static_cast<Health>(r.u8()); bd.readiness = static_cast<Readiness>(r.u8()); bd.freshness = static_cast<Freshness>(r.u8()); bd.provenance = get_prov(r);
  return bd;
}

void put_device(std::vector<std::uint8_t>& b, const DeviceDescriptor& d) {
  put_id(b, d.device_id.value()); put_id(b, d.generation.value()); put_str(b, d.name); put_str(b, d.vendor); put_str(b, d.architecture); put_str(b, d.compute_capability);
  put_u64(b, static_cast<std::uint64_t>(d.total_memory)); put_u64(b, static_cast<std::uint64_t>(d.free_memory)); put_str(b, d.backend_binding); put_prov(b, d.provenance); put_u8(b, static_cast<std::uint8_t>(d.freshness)); put_u8(b, static_cast<std::uint8_t>(d.health));
}
DeviceDescriptor get_device(R& r) {
  DeviceDescriptor d; d.device_id = DeviceId(get_id(r)); d.generation = DeviceGeneration(get_id(r)); d.name = r.str(); d.vendor = r.str(); d.architecture = r.str(); d.compute_capability = r.str();
  d.total_memory = static_cast<std::int64_t>(r.u64()); d.free_memory = static_cast<std::int64_t>(r.u64()); d.backend_binding = r.str(); d.provenance = get_prov(r); d.freshness = static_cast<Freshness>(r.u8()); d.health = static_cast<Health>(r.u8());
  return d;
}

void put_capability(std::vector<std::uint8_t>& b, const CapabilityDescriptor& c) {
  put_id(b, c.capability_id.value()); put_u8(b, static_cast<std::uint8_t>(c.kind)); put_id(b, c.generation.value()); put_version(b, c.version); put_capval(b, c.value);
  put_u32(b, static_cast<std::uint32_t>(c.attributes.size())); for (auto& a : c.attributes) put_str(b, a);
  put_u32(b, static_cast<std::uint32_t>(c.constraints.size())); for (auto& a : c.constraints) put_str(b, a);
  put_prov(b, c.provenance); put_u8(b, static_cast<std::uint8_t>(c.freshness)); put_id(b, c.authority_generation.value());
}
CapabilityDescriptor get_capability(R& r) {
  CapabilityDescriptor c; c.capability_id = CapabilityId(get_id(r)); c.kind = static_cast<CapabilityKind>(r.u8()); c.generation = CapabilityGeneration(get_id(r)); c.version = get_version(r); c.value = get_capval(r);
  std::uint32_t na = r.u32(); if (na > 256) r.ok = false; for (std::uint32_t k = 0; k < na && r.ok; ++k) c.attributes.push_back(r.str());
  std::uint32_t nc = r.u32(); if (nc > 256) r.ok = false; for (std::uint32_t k = 0; k < nc && r.ok; ++k) c.constraints.push_back(r.str());
  c.provenance = get_prov(r); c.freshness = static_cast<Freshness>(r.u8()); c.authority_generation = AuthorityGeneration(get_id(r));
  return c;
}

void put_node(std::vector<std::uint8_t>& b, const NodeDescriptor& n) {
  put_id(b, n.node_id.value()); put_id(b, n.generation.value()); put_str(b, n.hostname);
  put_u32(b, static_cast<std::uint32_t>(n.runtimes.size())); for (auto e : n.runtimes) put_id(b, e.value());
  put_u32(b, static_cast<std::uint32_t>(n.service_instances.size())); for (auto e : n.service_instances) put_id(b, e.value());
  put_u32(b, static_cast<std::uint32_t>(n.devices.size())); for (auto e : n.devices) put_id(b, e.value());
  put_u32(b, static_cast<std::uint32_t>(n.backends.size())); for (auto e : n.backends) put_id(b, e.value());
  put_u8(b, static_cast<std::uint8_t>(n.health)); put_u8(b, static_cast<std::uint8_t>(n.readiness)); put_u8(b, static_cast<std::uint8_t>(n.freshness)); put_u8(b, static_cast<std::uint8_t>(n.reachability)); put_prov(b, n.provenance);
}
NodeDescriptor get_node(R& r) {
  NodeDescriptor n; n.node_id = NodeId(get_id(r)); n.generation = NodeGeneration(get_id(r)); n.hostname = r.str();
  std::uint32_t k1 = r.u32(); if (k1 > 4096) r.ok = false; for (std::uint32_t i = 0; i < k1 && r.ok; ++i) n.runtimes.push_back(RuntimeInstanceId(get_id(r)));
  std::uint32_t k2 = r.u32(); if (k2 > 4096) r.ok = false; for (std::uint32_t i = 0; i < k2 && r.ok; ++i) n.service_instances.push_back(ServiceInstanceId(get_id(r)));
  std::uint32_t k3 = r.u32(); if (k3 > 4096) r.ok = false; for (std::uint32_t i = 0; i < k3 && r.ok; ++i) n.devices.push_back(DeviceId(get_id(r)));
  std::uint32_t k4 = r.u32(); if (k4 > 4096) r.ok = false; for (std::uint32_t i = 0; i < k4 && r.ok; ++i) n.backends.push_back(BackendId(get_id(r)));
  n.health = static_cast<Health>(r.u8()); n.readiness = static_cast<Readiness>(r.u8()); n.freshness = static_cast<Freshness>(r.u8()); n.reachability = static_cast<Reachability>(r.u8()); n.provenance = get_prov(r);
  return n;
}

void put_lease(std::vector<std::uint8_t>& b, const LeaseDescriptor& l) {
  put_id(b, l.lease_id.value()); put_id(b, l.generation.value()); put_id(b, l.service_instance.value()); put_id(b, l.boot.value());
  put_u64(b, static_cast<std::uint64_t>(l.issued_ms)); put_u64(b, static_cast<std::uint64_t>(l.renew_interval_ms)); put_u8(b, static_cast<std::uint8_t>(l.renewal_policy)); put_u8(b, static_cast<std::uint8_t>(l.state));
  put_u64(b, static_cast<std::uint64_t>(l.expires_ms)); put_prov(b, l.provenance); put_id(b, l.authority_generation.value());
}
LeaseDescriptor get_lease(R& r) {
  LeaseDescriptor l; l.lease_id = LeaseId(get_id(r)); l.generation = LeaseGeneration(get_id(r)); l.service_instance = ServiceInstanceId(get_id(r)); l.boot = WorkerBootId(get_id(r));
  l.issued_ms = static_cast<std::int64_t>(r.u64()); l.renew_interval_ms = static_cast<std::int64_t>(r.u64()); l.renewal_policy = static_cast<LeaseRenewalPolicy>(r.u8()); l.state = static_cast<LeaseState>(r.u8());
  l.expires_ms = static_cast<std::int64_t>(r.u64()); l.provenance = get_prov(r); l.authority_generation = AuthorityGeneration(get_id(r));
  return l;
}

void put_tombstone(std::vector<std::uint8_t>& b, const TombstoneRecord& t) {
  put_id(b, t.tombstone_id.value()); put_str(b, t.target_kind); put_str(b, t.target_text); put_id(b, t.generation_floor.value());
  put_id(b, t.epoch.value()); put_id(b, t.boot.value()); put_id(b, t.authority_generation.value()); put_str(b, t.reason); put_u64(b, static_cast<std::uint64_t>(t.timestamp_ms)); put_prov(b, t.provenance);
}
TombstoneRecord get_tombstone(R& r) {
  TombstoneRecord t; t.tombstone_id = TombstoneId(get_id(r)); t.target_kind = r.str(); t.target_text = r.str(); t.generation_floor = RecordGeneration(get_id(r));
  t.epoch = CoordinatorEpoch(get_id(r)); t.boot = WorkerBootId(get_id(r)); t.authority_generation = AuthorityGeneration(get_id(r)); t.reason = r.str(); t.timestamp_ms = static_cast<std::int64_t>(r.u64()); t.provenance = get_prov(r);
  return t;
}

void put_invalidation(std::vector<std::uint8_t>& b, const InvalidationRecord& iv) {
  put_str(b, iv.target_kind); put_str(b, iv.target_text); put_id(b, iv.generation.value()); put_id(b, iv.epoch.value()); put_id(b, iv.boot.value()); put_id(b, iv.authority_generation.value()); put_str(b, iv.reason); put_u64(b, static_cast<std::uint64_t>(iv.timestamp_ms)); put_prov(b, iv.provenance);
}
InvalidationRecord get_invalidation(R& r) {
  InvalidationRecord iv; iv.target_kind = r.str(); iv.target_text = r.str(); iv.generation = RecordGeneration(get_id(r)); iv.epoch = CoordinatorEpoch(get_id(r)); iv.boot = WorkerBootId(get_id(r)); iv.authority_generation = AuthorityGeneration(get_id(r)); iv.reason = r.str(); iv.timestamp_ms = static_cast<std::int64_t>(r.u64()); iv.provenance = get_prov(r);
  return iv;
}

void put_compat(std::vector<std::uint8_t>& b, const CompatibilityRecord& c) {
  put_id(b, c.compatibility_id.value()); put_id(b, c.generation.value()); put_u8(b, static_cast<std::uint8_t>(c.outcome)); put_u8(b, static_cast<std::uint8_t>(c.freshness)); put_prov(b, c.provenance);
}
CompatibilityRecord get_compat(R& r) {
  CompatibilityRecord c; c.compatibility_id = CompatibilityId(get_id(r)); c.generation = CompatibilityGeneration(get_id(r)); c.outcome = static_cast<CompatibilityOutcome>(r.u8()); c.freshness = static_cast<Freshness>(r.u8()); c.provenance = get_prov(r);
  return c;
}

}  // anonymous namespace

template <typename K, typename V, typename Put>
void serialize_sorted(const std::unordered_map<K, V>& m, std::vector<std::uint8_t>& out, Put put) {
  std::vector<K> keys; keys.reserve(m.size());
  for (const auto& kv : m) keys.push_back(kv.first);
  std::sort(keys.begin(), keys.end(), [](const K& a, const K& b) { return a.value() < b.value(); });
  for (const auto& k : keys) put(out, m.at(k));
}

std::vector<std::uint8_t> Registry::serialize() const { std::vector<std::uint8_t> out; serialize_to(out); return out; }

void Registry::serialize_to(std::vector<std::uint8_t>& out) const {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  out.clear();
  put_u32(out, kPersistMagic);
  put_u8(out, kPersistVersion);
  put_u64(out, authority_.epoch.value()); put_u64(out, authority_.registry_generation.value()); put_u64(out, authority_.record_generation.value());
  put_u64(out, authority_.service_gen.value()); put_u64(out, authority_.instance_gen.value()); put_u64(out, authority_.runtime_gen.value()); put_u64(out, authority_.runtime_instance_gen.value());
  put_u64(out, authority_.endpoint_gen.value()); put_u64(out, authority_.protocol_gen.value()); put_u64(out, authority_.backend_gen.value()); put_u64(out, authority_.device_gen.value());
  put_u64(out, authority_.capability_gen.value()); put_u64(out, authority_.lease_gen.value()); put_u64(out, authority_.node_gen.value()); put_u64(out, authority_.compat_gen.value());
  put_u64(out, authority_.trust_gen.value()); put_u64(out, authority_.authority_gen.value());
  put_u64(out, next_boot_); put_u64(out, next_id_);
  put_u32(out, static_cast<std::uint32_t>(services_.size())); put_u32(out, static_cast<std::uint32_t>(instances_.size()));
  put_u32(out, static_cast<std::uint32_t>(runtimes_.size())); put_u32(out, static_cast<std::uint32_t>(runtime_instances_.size()));
  put_u32(out, static_cast<std::uint32_t>(endpoints_.size())); put_u32(out, static_cast<std::uint32_t>(protocols_.size()));
  put_u32(out, static_cast<std::uint32_t>(backends_.size())); put_u32(out, static_cast<std::uint32_t>(devices_.size()));
  put_u32(out, static_cast<std::uint32_t>(capabilities_.size())); put_u32(out, static_cast<std::uint32_t>(nodes_.size()));
  put_u32(out, static_cast<std::uint32_t>(leases_.size())); put_u32(out, static_cast<std::uint32_t>(tombstones_.size()));
  put_u32(out, static_cast<std::uint32_t>(compat_records_.size())); put_u32(out, static_cast<std::uint32_t>(invalidations_.size()));
  put_u32(out, static_cast<std::uint32_t>(instance_history_.size())); put_u32(out, static_cast<std::uint32_t>(runtime_history_.size()));
  put_u32(out, static_cast<std::uint32_t>(endpoint_history_.size())); put_u32(out, static_cast<std::uint32_t>(capability_history_.size()));

  serialize_sorted(services_, out, put_servicedesc);
  serialize_sorted(instances_, out, put_instance);
  serialize_sorted(runtimes_, out, put_runtime);
  serialize_sorted(runtime_instances_, out, put_runtime);
  serialize_sorted(endpoints_, out, put_endpoint);
  serialize_sorted(protocols_, out, put_protocol);
  serialize_sorted(backends_, out, put_backend);
  serialize_sorted(devices_, out, put_device);
  serialize_sorted(capabilities_, out, put_capability);
  serialize_sorted(nodes_, out, put_node);
  serialize_sorted(leases_, out, put_lease);
  serialize_sorted(tombstones_, out, put_tombstone);
  serialize_sorted(compat_records_, out, put_compat);
  // invalidations vector (already deterministic insertion order)
  // (invalidations count already emitted in header; records follow)
  for (const auto& iv : invalidations_) put_invalidation(out, iv);
  for (const auto& i : instance_history_) put_instance(out, i);
  for (const auto& r2 : runtime_history_) put_runtime(out, r2);
  for (const auto& e : endpoint_history_) put_endpoint(out, e);
  for (const auto& c : capability_history_) put_capability(out, c);

  Sha256 d = sha256(std::string_view(reinterpret_cast<const char*>(out.data()), out.size()));
  std::uint32_t crc = crc32(std::string_view(reinterpret_cast<const char*>(out.data()), out.size()));
  put_u32(out, crc);
  out.insert(out.end(), d.begin(), d.end());
}

std::string Registry::semantic_digest() const {
  std::vector<std::uint8_t> b; serialize_to(b);
  return sha256_hex(std::string_view(reinterpret_cast<const char*>(b.data()), b.size()));
}

void Registry::load_from(const std::uint8_t* data, std::size_t size) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  if (data == nullptr || size < 46) throw RegistryError(ErrorKind::MALFORMED, "persistence too short");
  std::size_t content_len = size - 36;
  std::uint32_t stored_crc = 0;
  for (int i = 0; i < 4; ++i) stored_crc |= static_cast<std::uint32_t>(data[content_len + i]) << (8 * i);
  Sha256 stored_sha; std::memcpy(stored_sha.data(), data + content_len + 4, 32);
  std::uint32_t computed_crc = crc32(std::string_view(reinterpret_cast<const char*>(data), content_len));
  if (computed_crc != stored_crc) throw RegistryError(ErrorKind::MALFORMED, "persistence CRC mismatch");
  Sha256 computed_sha = sha256(std::string_view(reinterpret_cast<const char*>(data), content_len));
  if (computed_sha != stored_sha) throw RegistryError(ErrorKind::MALFORMED, "persistence semantic digest mismatch");

  R r(data, content_len);
  if (r.u32() != kPersistMagic) throw RegistryError(ErrorKind::MALFORMED, "persistence bad magic");
  if (r.u8() != kPersistVersion) throw RegistryError(ErrorKind::MALFORMED, "persistence unsupported version");
  std::uint64_t epoch = r.u64(); std::uint64_t reg_gen = r.u64(); std::uint64_t rec_gen = r.u64();
  std::uint64_t s_gen = r.u64(), i_gen = r.u64(), rt_gen = r.u64(), rti_gen = r.u64();
  std::uint64_t ep_gen = r.u64(), pr_gen = r.u64(), b_gen = r.u64(), d_gen = r.u64();
  std::uint64_t c_gen = r.u64(), l_gen = r.u64(), n_gen = r.u64(), cmp_gen = r.u64();
  std::uint64_t t_gen = r.u64(), au_gen = r.u64();
  std::uint64_t nb = r.u64(); std::uint64_t nid = r.u64();
  auto rd_count = [&]() { std::uint32_t c = r.u32(); if (c > kMaxRecordsPerMap) { r.ok = false; } return c; };
  std::uint32_t n_services = rd_count(); std::uint32_t n_instances = rd_count();
  std::uint32_t n_runtimes = rd_count(); std::uint32_t n_rtinst = rd_count();
  std::uint32_t n_endpoints = rd_count(); std::uint32_t n_protocols = rd_count();
  std::uint32_t n_backends = rd_count(); std::uint32_t n_devices = rd_count();
  std::uint32_t n_capabilities = rd_count(); std::uint32_t n_nodes = rd_count();
  std::uint32_t n_leases = rd_count(); std::uint32_t n_tombstones = rd_count();
  std::uint32_t n_compat = rd_count(); std::uint32_t n_invalidations = rd_count();
  std::uint32_t n_ih = rd_count(); std::uint32_t n_rh = rd_count(); std::uint32_t n_eh = rd_count(); std::uint32_t n_ch = rd_count();
  if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "impossible counts");

  services_.clear(); instances_.clear(); runtimes_.clear(); runtime_instances_.clear(); endpoints_.clear(); protocols_.clear();
  backends_.clear(); devices_.clear(); capabilities_.clear(); nodes_.clear(); leases_.clear(); tombstones_.clear();
  compat_records_.clear(); invalidations_.clear(); instance_history_.clear(); runtime_history_.clear(); endpoint_history_.clear(); capability_history_.clear();

  for (std::uint32_t i = 0; i < n_services; ++i) { ServiceDescriptor s = get_servicedesc(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed service record"); if (services_.count(s.service_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate service identity"); if (static_cast<std::uint8_t>(s.kind) > 16) throw RegistryError(ErrorKind::MALFORMED, "invalid service kind"); services_[s.service_id] = s; }
  for (std::uint32_t i = 0; i < n_instances; ++i) { ServiceInstance in = get_instance(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed instance record"); if (instances_.count(in.instance_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate instance identity"); instances_[in.instance_id] = in; }
  for (std::uint32_t i = 0; i < n_runtimes; ++i) { RuntimeDescriptor rt = get_runtime(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed runtime record"); if (runtimes_.count(rt.runtime_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate runtime identity"); runtimes_[rt.runtime_id] = rt; }
  for (std::uint32_t i = 0; i < n_rtinst; ++i) { RuntimeDescriptor rt = get_runtime(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed runtime instance record"); if (runtime_instances_.count(rt.runtime_instance)) throw RegistryError(ErrorKind::MALFORMED, "duplicate runtime instance identity"); runtime_instances_[rt.runtime_instance] = rt; }
  for (std::uint32_t i = 0; i < n_endpoints; ++i) { EndpointDescriptor e = get_endpoint(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed endpoint record"); if (endpoints_.count(e.endpoint_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate endpoint identity"); endpoints_[e.endpoint_id] = e; }
  for (std::uint32_t i = 0; i < n_protocols; ++i) { ProtocolDescriptor p = get_protocol(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed protocol record"); if (protocols_.count(p.protocol_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate protocol identity"); protocols_[p.protocol_id] = p; }
  for (std::uint32_t i = 0; i < n_backends; ++i) { BackendDescriptor bd = get_backend(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed backend record"); if (backends_.count(bd.backend_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate backend identity"); backends_[bd.backend_id] = bd; }
  for (std::uint32_t i = 0; i < n_devices; ++i) { DeviceDescriptor d = get_device(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed device record"); if (devices_.count(d.device_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate device identity"); devices_[d.device_id] = d; }
  for (std::uint32_t i = 0; i < n_capabilities; ++i) { CapabilityDescriptor c = get_capability(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed capability record"); if (capabilities_.count(c.capability_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate capability identity"); capabilities_[c.capability_id] = c; }
  for (std::uint32_t i = 0; i < n_nodes; ++i) { NodeDescriptor n = get_node(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed node record"); if (nodes_.count(n.node_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate node identity"); nodes_[n.node_id] = n; }
  for (std::uint32_t i = 0; i < n_leases; ++i) { LeaseDescriptor l = get_lease(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed lease record"); if (leases_.count(l.lease_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate lease identity"); leases_[l.lease_id] = l; }
  for (std::uint32_t i = 0; i < n_tombstones; ++i) { TombstoneRecord t = get_tombstone(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed tombstone record"); if (tombstones_.count(t.tombstone_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate tombstone identity"); tombstones_[t.tombstone_id] = t; }
  for (std::uint32_t i = 0; i < n_compat; ++i) { CompatibilityRecord c = get_compat(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed compatibility record"); if (compat_records_.count(c.compatibility_id)) throw RegistryError(ErrorKind::MALFORMED, "duplicate compatibility identity"); compat_records_[c.compatibility_id] = c; }
  for (std::uint32_t i = 0; i < n_invalidations; ++i) { InvalidationRecord iv = get_invalidation(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed invalidation record"); invalidations_.push_back(iv); }
  for (std::uint32_t i = 0; i < n_ih; ++i) { ServiceInstance in = get_instance(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed history instance"); instance_history_.push_back(in); }
  for (std::uint32_t i = 0; i < n_rh; ++i) { RuntimeDescriptor rt = get_runtime(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed history runtime"); runtime_history_.push_back(rt); }
  for (std::uint32_t i = 0; i < n_eh; ++i) { EndpointDescriptor e = get_endpoint(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed history endpoint"); endpoint_history_.push_back(e); }
  for (std::uint32_t i = 0; i < n_ch; ++i) { CapabilityDescriptor c = get_capability(r); if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "malformed history capability"); capability_history_.push_back(c); }
  if (!r.ok) throw RegistryError(ErrorKind::MALFORMED, "truncated/malformed persistence record");
  if (r.pos != content_len) throw RegistryError(ErrorKind::MALFORMED, "trailing garbage in persistence");

  authority_.epoch = CoordinatorEpoch{epoch}; authority_.registry_generation = RegistryGeneration{reg_gen}; authority_.record_generation = RecordGeneration{rec_gen};
  authority_.service_gen = ServiceGeneration{s_gen}; authority_.instance_gen = ServiceInstanceGeneration{i_gen}; authority_.runtime_gen = RuntimeGeneration{rt_gen}; authority_.runtime_instance_gen = RuntimeInstanceGeneration{rti_gen};
  authority_.endpoint_gen = EndpointGeneration{ep_gen}; authority_.protocol_gen = ProtocolGeneration{pr_gen}; authority_.backend_gen = BackendGeneration{b_gen}; authority_.device_gen = DeviceGeneration{d_gen};
  authority_.capability_gen = CapabilityGeneration{c_gen}; authority_.lease_gen = LeaseGeneration{l_gen}; authority_.node_gen = NodeGeneration{n_gen}; authority_.compat_gen = CompatibilityGeneration{cmp_gen};
  authority_.trust_gen = TrustGeneration{t_gen}; authority_.authority_gen = AuthorityGeneration{au_gen};
  next_boot_ = nb; next_id_ = nid;

  // Recovery: clear live process authority; require revalidation of physical
  // liveness. Unlike a coordinator bootstrap, this does not advance the epoch,
  // so a serialize/load round trip remains deterministic.
  authority_.live_boots.clear();
  authority_.current_boot.clear();
  authority_.boot_worker.clear();
  for (auto& kv : instances_) {
    ServiceInstance& inst = kv.second;
    if (inst.lifecycle == Lifecycle::AVAILABLE || inst.lifecycle == Lifecycle::DEGRADED ||
        inst.lifecycle == Lifecycle::UNREADY) {
      inst.freshness = Freshness::REVALIDATION_REQUIRED;
      inst.reachability = Reachability::REVALIDATION_REQUIRED;
      if (inst.lifecycle == Lifecycle::AVAILABLE || inst.lifecycle == Lifecycle::DEGRADED) inst.lifecycle = Lifecycle::UNREADY;
    }
    inst.updated_ms = 0;
  }
  for (auto& kv : endpoints_) {
    EndpointDescriptor& ep = kv.second;
    if (ep.transport == TransportKind::IN_PROCESS || ep.transport == TransportKind::LOCAL_IPC) {
      ep.freshness = Freshness::REVALIDATION_REQUIRED; ep.reachability = Reachability::REVALIDATION_REQUIRED;
    } else if (ep.transport == TransportKind::TCP) { ep.reachability = Reachability::REVALIDATION_REQUIRED; }
  }
  for (auto& kv : capabilities_) {
    CapabilityDescriptor& c = kv.second;
    if (c.freshness == Freshness::CURRENT && c.provenance.is_physical()) c.freshness = Freshness::REVALIDATION_REQUIRED;
  }
  for (auto& kv : leases_) {
    LeaseDescriptor& l = kv.second;
    if (l.state == LeaseState::ACTIVE) l.state = LeaseState::REVALIDATION_REQUIRED;
  }
  rebuild_indexes();

  // Rebuild accounting from canonical records.
  accounting_ = Accounting{};
  accounting_.services = static_cast<std::int64_t>(services_.size());
  accounting_.service_instances = static_cast<std::int64_t>(instances_.size());
  accounting_.runtimes = static_cast<std::int64_t>(runtimes_.size());
  accounting_.runtime_instances = static_cast<std::int64_t>(runtime_instances_.size());
  accounting_.nodes = static_cast<std::int64_t>(nodes_.size());
  accounting_.endpoints = static_cast<std::int64_t>(endpoints_.size());
  accounting_.protocols = static_cast<std::int64_t>(protocols_.size());
  accounting_.backends = static_cast<std::int64_t>(backends_.size());
  accounting_.devices = static_cast<std::int64_t>(devices_.size());
  accounting_.capabilities = static_cast<std::int64_t>(capabilities_.size());
  accounting_.leases = static_cast<std::int64_t>(leases_.size());
  accounting_.available_instances = 0; accounting_.stale_instances = 0; accounting_.unreachable_instances = 0;
  for (const auto& kv : instances_) { if (kv.second.lifecycle == Lifecycle::AVAILABLE) ++accounting_.available_instances; else if (kv.second.lifecycle == Lifecycle::STALE) ++accounting_.stale_instances; else if (kv.second.lifecycle == Lifecycle::UNREACHABLE) ++accounting_.unreachable_instances; }
}

void serialize_registry(const Registry& reg, std::vector<std::uint8_t>& out) { reg.serialize_to(out); }
std::string registry_semantic_digest(const Registry& reg) { return reg.semantic_digest(); }
void load_registry(Registry& reg, const std::uint8_t* data, std::size_t size) { reg.load_from(data, size); }

void Registry::load_from(const std::vector<std::uint8_t>& data) { load_from(data.data(), data.size()); }

}  // namespace runtimeregistry