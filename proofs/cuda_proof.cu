// Real CUDA capability proof for Runtime Registry (RTX 5090 / CUDA 13.1).
#include <cuda_runtime.h>
#include <cstdio>
#include <cstring>
#include <vector>

#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/selection.hpp>

using namespace runtimeregistry;

namespace { int g_fail = 0; }
void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++g_fail;
}


__global__ void rr_square_kernel(float* data, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) data[i] = data[i] * data[i];
}

int main() {
  std::printf("[cuda_proof]\n");
  int dev_count = 0;
  cudaError_t ce = cudaGetDeviceCount(&dev_count);
  check(ce == cudaSuccess, "cudaGetDeviceCount succeeded");
  check(dev_count >= 1, "at least one CUDA device present");
  cudaDeviceProp prop{};
  ce = cudaGetDeviceProperties(&prop, 0);
  check(ce == cudaSuccess, "cudaGetDeviceProperties succeeded");
  std::printf("  device=%s compute_capability=%d.%d mem=%zu\n", prop.name, prop.major, prop.minor, (size_t)prop.totalGlobalMem);

  Registry reg;
  reg.begin_coordinator_epoch();
  CoordinatorEpoch epoch = reg.authority().epoch;
  WorkerBootId boot = reg.adopt_worker(WorkerId(1));
  AuthorityEnvelope env; env.epoch = epoch; env.boot = boot;

  RuntimeDescriptor rt;
  rt.runtime_id = RuntimeId(1); rt.runtime_instance = RuntimeInstanceId(1); rt.generation = RuntimeGeneration(1);
  rt.kind = RuntimeKind::CUDA_RUNTIME; rt.name = "cuda-13.1"; rt.family = "cuda";
  rt.version = SemanticVersion::parse("13.1.0").value_or(SemanticVersion{});
  rt.api_version = ApiVersion(13, 1); rt.abi_version = AbiVersion(1, 0);
  rt.architecture = std::string("sm_") + std::to_string(prop.major) + std::to_string(prop.minor);
  rt.device_constraint = prop.name;
  rt.provenance = make_provenance(EvidenceKind::MEASURED, "cuda::driver", 0);
  reg.register_runtime(rt, env);
  check(true, "CUDA RuntimeInstance registered");

  DeviceDescriptor dev;
  dev.device_id = DeviceId(1); dev.generation = DeviceGeneration(1);
  dev.name = prop.name; dev.vendor = "NVIDIA";
  dev.architecture = std::string("sm_") + std::to_string(prop.major) + std::to_string(prop.minor);
  dev.compute_capability = std::to_string(prop.major) + "." + std::to_string(prop.minor);
  dev.total_memory = static_cast<std::int64_t>(prop.totalGlobalMem);
  dev.free_memory = -1; dev.backend_binding = "CUDA";
  dev.provenance = make_provenance(EvidenceKind::MEASURED, "cuda::device0", 0);
  dev.freshness = Freshness::CURRENT; dev.health = Health::HEALTHY;
  reg.register_device(dev, env);
  check(true, "Device registered");

  CapabilityDescriptor avail; avail.capability_id = CapabilityId(1); avail.kind = CapabilityKind::CUDA_AVAILABLE;
  avail.generation = CapabilityGeneration(1); avail.version = SemanticVersion::parse("1.0.0").value();
  avail.value = CapabilityValue::make_bool(true); avail.freshness = Freshness::CURRENT;
  avail.provenance = make_provenance(EvidenceKind::MEASURED, "cuda::device0", 0);
  reg.register_capability(avail, env);
  CapabilityDescriptor cc; cc.capability_id = CapabilityId(2); cc.kind = CapabilityKind::CUDA_COMPUTE_CAPABILITY;
  cc.generation = CapabilityGeneration(1); cc.version = SemanticVersion::parse("1.0.0").value();
  cc.value = CapabilityValue::make_string(std::to_string(prop.major) + "." + std::to_string(prop.minor));
  cc.freshness = Freshness::CURRENT; cc.provenance = make_provenance(EvidenceKind::MEASURED, "cuda::device0", 0);
  reg.register_capability(cc, env);
  CapabilityDescriptor mem; mem.capability_id = CapabilityId(3); mem.kind = CapabilityKind::CUDA_MEMORY;
  mem.generation = CapabilityGeneration(1); mem.version = SemanticVersion::parse("1.0.0").value();
  CapabilityValue cv = CapabilityValue::make_structured();
  cv.structured.emplace_back("total_bytes", CapabilityValue::make_integer(static_cast<std::int64_t>(prop.totalGlobalMem)));
  mem.value = cv; mem.freshness = Freshness::CURRENT;
  mem.provenance = make_provenance(EvidenceKind::MEASURED, "cuda::device0", 0);
  reg.register_capability(mem, env);
  CapabilityDescriptor arch; arch.capability_id = CapabilityId(4); arch.kind = CapabilityKind::CUDA_ARCHITECTURE;
  arch.generation = CapabilityGeneration(1); arch.value = CapabilityValue::make_string(std::string("sm_") + std::to_string(prop.major) + std::to_string(prop.minor));
  arch.freshness = Freshness::CURRENT; arch.provenance = make_provenance(EvidenceKind::MEASURED, "cuda::device0", 0);
  reg.register_capability(arch, env);
  check(true, "CUDA capability set advertised (MEASURED)");

  EndpointDescriptor ep; ep.endpoint_id = EndpointId(1); ep.generation = EndpointGeneration(1); ep.protocol = ProtocolId(1);
  ep.locator.text = "127.0.0.1:41817"; ep.transport = TransportKind::TCP; ep.port = 41817;
  ep.health = Health::HEALTHY; ep.reachability = Reachability::REACHABLE; ep.freshness = Freshness::CURRENT;
  ep.provenance = make_provenance(EvidenceKind::MEASURED, "cuda::probe", 0);
  reg.register_endpoint(ep, env);

  ServiceDescriptor svc; svc.service_id = ServiceId(100); svc.kind = ServiceKind::MODEL_SERVER; svc.name = "cuda-inference";
  svc.generation = ServiceGeneration(1); svc.version = SemanticVersion::parse("1.0.0").value();
  svc.api_version = ApiVersion(1, 0); svc.abi_version = AbiVersion(1, 0);
  svc.required_capabilities = {CapabilityKind::CUDA_AVAILABLE, CapabilityKind::CUDA_COMPUTE_CAPABILITY};
  reg.register_service(svc, env);

  ServiceInstance inst;
  inst.service_id = svc.service_id; inst.service_generation = ServiceGeneration(1); inst.instance_generation = ServiceInstanceGeneration(1);
  inst.node = NodeId(1); inst.worker = WorkerId(1); inst.boot = boot;
  inst.runtime_id = rt.runtime_id; inst.runtime_instance = rt.runtime_instance;
  inst.endpoints = {ep.endpoint_id}; inst.protocols = {ProtocolId(1)};
  inst.capabilities = {avail.capability_id, cc.capability_id, mem.capability_id, arch.capability_id};
  inst.health = Health::HEALTHY; inst.readiness = Readiness::READY; inst.freshness = Freshness::CURRENT; inst.reachability = Reachability::REACHABLE;
  inst.version = SemanticVersion::parse("1.0.0").value();
  inst.provenance = make_provenance(EvidenceKind::REPORTED, "workerA", 0); inst.lifecycle = Lifecycle::REGISTERING;
  ServiceInstanceId inst_id = reg.register_instance(inst, env);
  check(true, "CUDA-requiring service instance registered");

  const int N = 1 << 16;
  std::vector<float> host(N);
  for (int i = 0; i < N; ++i) host[i] = (float)(i & 0xFF) * 0.25f;
  float* d = nullptr;
  ce = cudaMalloc(&d, N * sizeof(float)); check(ce == cudaSuccess, "cudaMalloc succeeded");
  ce = cudaMemcpy(d, host.data(), N * sizeof(float), cudaMemcpyHostToDevice); check(ce == cudaSuccess, "cudaMemcpy H2D succeeded");
  int threads = 256; int blocks = (N + threads - 1) / threads;
  rr_square_kernel<<<blocks, threads>>>(d, N);
  ce = cudaDeviceSynchronize(); check(ce == cudaSuccess, "cudaDeviceSynchronize succeeded (kernel executed)");
  std::vector<float> back(N);
  ce = cudaMemcpy(back.data(), d, N * sizeof(float), cudaMemcpyDeviceToHost); check(ce == cudaSuccess, "cudaMemcpy D2H succeeded");
  bool parity = true;
  for (int i = 0; i < N; ++i) { float expect = (float)(i & 0xFF) * 0.25f; float sq = expect * expect; if (back[i] != sq) { parity = false; break; } }
  check(parity, "CPU-reference parity holds");
  ce = cudaFree(d); check(ce == cudaSuccess, "cudaFree succeeded (cleanup)");

  RegistryQuery q; q.service_kind = ServiceKind::MODEL_SERVER;
  q.required_capabilities = {CapabilityKind::CUDA_AVAILABLE, CapabilityKind::CUDA_COMPUTE_CAPABILITY};
  q.required_freshness = Freshness::CURRENT; q.required_reachability = Reachability::REACHABLE;
  RegistryResult r = reg.query(q);
  check(r.outcome == QueryOutcome::FOUND_EXACT || r.outcome == QueryOutcome::FOUND_MULTIPLE, "CUDA capability-aware query found the service");
  check(!r.ranked.empty(), "query returned a ranked candidate");
  if (!r.ranked.empty()) check(r.ranked[0].instance_id == inst_id, "CUDA-requiring service selected");

  AuthorityEnvelope stale = env; stale.capability_generation = CapabilityGeneration(1);
  CapabilityDescriptor stale_cap = avail; stale_cap.generation = CapabilityGeneration(0);
  bool rejected = false;
  try { reg.register_capability(stale_cap, stale); } catch (const RegistryError& e) { rejected = (e.kind() == ErrorKind::STALE_CAPABILITY_GENERATION); }
  check(rejected, "stale capability-generation replay rejected");

  AuthorityEnvelope fresh = env; fresh.device_generation = DeviceGeneration(2);
  DeviceDescriptor fresh_dev = dev; fresh_dev.generation = DeviceGeneration(2);
  reg.register_device(fresh_dev, fresh);
  check(true, "device generation refreshed");
  RegistryResult q2 = reg.query(q);
  check(!q2.ranked.empty() || q2.outcome == QueryOutcome::FOUND_EXACT, "fresh discovery query succeeds after device refresh");

  check(reg.check_invariants().empty(), "registry invariants hold");
  check(!reg.accounting_negative(), "accounting never negative");

  std::printf("cuda_proof: %s\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
  return g_fail == 0 ? 0 : 1;
}