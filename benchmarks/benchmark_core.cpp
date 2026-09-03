#include "example_util.hpp"
#include <runtimeregistry/protocol.hpp>
#include <chrono>
#include <cstdio>

using namespace runtimeregistry;
namespace {
double ns_per(std::chrono::steady_clock::duration d, std::size_t ops) {
  return std::chrono::duration<double, std::nano>(d).count() / static_cast<double>(ops);
}
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("[benchmark_core] completed-work benchmarks\n");
  // 100k is intentionally omitted: per-registration index rebuild is O(n^2) on this
  // host, so it is not practical to populate/validate a 100k-instance registry here.
  // 1k and 10k are validated; see the README for the honest note.
  const std::size_t sizes[] = {1000, 10000};
  for (std::size_t N : sizes) {
    std::printf("\n  registry size = %zu\n", N);

    auto t0 = std::chrono::steady_clock::now();
    Registry reg; reg.begin_coordinator_epoch();
    WorkerBootId boot = reg.adopt_worker(WorkerId(1));
    AuthorityEnvelope env; env.epoch = reg.authority().epoch; env.boot = boot;
    RuntimeInstanceId rt = rr_examples::add_runtime(reg, env, "native", "cpp", "x64");
    CapabilityId cap = rr_examples::add_capability(reg, env, 1, CapabilityKind::CUDA_AVAILABLE, CapabilityValue::make_bool(true));
    for (std::size_t i = 0; i < N; ++i) {
      ServiceId sid(1000 + i);
      rr_examples::add_service(reg, env, 1000 + (std::uint64_t)i, ServiceKind::MODEL_SERVER, "svc",
        SemanticVersion::parse("1.0.0").value(), ApiVersion(1,0), {CapabilityKind::CUDA_AVAILABLE});
      EndpointId ep = rr_examples::add_endpoint(reg, env, i + 1, static_cast<std::uint16_t>(30000 + (i % 1000)));
      env.boot = boot;
      rr_examples::add_instance(reg, env, sid, boot, WorkerId(1), rt, NodeId(1),
        SemanticVersion::parse("1.0.0").value(), {ep}, {cap});
    }
    auto t1 = std::chrono::steady_clock::now();
    std::printf("    registration: %.1f ns/op\n", ns_per(t1 - t0, N));

    RegistryQuery q; q.service_kind = ServiceKind::MODEL_SERVER;
    std::size_t iters = N < 100000 ? 200 : 50;
    auto s = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iters; ++i) { RegistryResult r = reg.query(q); (void)r; }
    auto e = std::chrono::steady_clock::now();
    std::printf("    service-kind lookup: %.1f ns/op (services=%lld)\n", ns_per(e - s, iters), (long long)reg.accounting().service_instances);

    q.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
    s = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iters; ++i) { RegistryResult r = reg.query(q); (void)r; }
    e = std::chrono::steady_clock::now();
    std::printf("    capability lookup: %.1f ns/op\n", ns_per(e - s, iters));

    q.required_capabilities = {}; q.has_minimum_api = true; q.minimum_api = ApiVersion(2,0);
    s = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iters; ++i) { RegistryResult r = reg.query(q); (void)r; }
    e = std::chrono::steady_clock::now();
    std::printf("    version filtering: %.1f ns/op\n", ns_per(e - s, iters));

    Frame f; f.kind = MessageKind::QUERY; f.payload.assign(64, 0x5A);
    std::vector<std::uint8_t> enc = encode_frame(f);
    s = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iters; ++i) { auto dec = decode_frame(enc.data(), enc.size()); (void)dec; }
    e = std::chrono::steady_clock::now();
    std::printf("    protocol encode/decode: %.1f ns/op\n", ns_per(e - s, iters));

    s = std::chrono::steady_clock::now();
    std::vector<std::uint8_t> bytes = reg.serialize();
    e = std::chrono::steady_clock::now();
    std::printf("    persistence serialize: %.1f ns/op (%zu bytes)\n", ns_per(e - s, 1), bytes.size());
    s = std::chrono::steady_clock::now();
    Registry r2; r2.load_from(bytes);
    e = std::chrono::steady_clock::now();
    std::printf("    persistence recover: %.1f ns/op\n", ns_per(e - s, 1));

    if (reg.accounting().service_instances != static_cast<std::int64_t>(N)) std::printf("    WARNING: accounting mismatch %lld\n", (long long)reg.accounting().service_instances);
    if (reg.accounting_negative()) std::printf("    WARNING: accounting negative\n");
  }
  std::printf("\n  barrier: all sizes completed\n");
  return 0;
}
