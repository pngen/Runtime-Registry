#include <runtimeregistry/registry.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include "test_util.hpp"
using namespace runtimeregistry;

void test_concurrent_registration_and_lookup() {
  Registry reg;
  reg.begin_coordinator_epoch();
  WorkerBootId b = reg.adopt_worker(WorkerId(1));
  AuthorityEnvelope env; env.epoch = reg.authority().epoch; env.boot = b;
  RuntimeDescriptor rt; rt.runtime_id=RuntimeId(1); rt.runtime_instance=RuntimeInstanceId(1); rt.generation=RuntimeGeneration(1);
  rt.kind=RuntimeKind::NATIVE_CPP; rt.name="r"; rt.family="cpp"; rt.version=SemanticVersion::parse("1.0.0").value();
  rt.api_version=ApiVersion(1,0); rt.abi_version=AbiVersion(1,0); reg.register_runtime(rt, env);

  const int NT = 4;      // writer threads
  const int N = 16;      // instances total (NT * per-thread)
  const int per = N / NT;
  std::atomic<int> thread_failures{0};

  std::vector<std::thread> writers;
  for (int t = 0; t < NT; ++t) {
    writers.emplace_back([&, t]() {
      try {
        ServiceId sid(300 + t);
        ServiceDescriptor svc; svc.service_id = sid; svc.kind = ServiceKind::OBSERVABILITY_SERVICE; svc.name = "svc";
        svc.generation = ServiceGeneration(1); svc.version = SemanticVersion::parse("1.0.0").value();
        svc.api_version = ApiVersion(1,0); svc.abi_version = AbiVersion(1,0);
        reg.register_service(svc, env);
        for (int i = 0; i < per; ++i) {
          EndpointDescriptor ep; ep.endpoint_id = EndpointId(0); ep.generation = EndpointGeneration(i+1);
          ep.protocol = ProtocolId(1); ep.locator.text = "127.0.0.1:5000"; ep.transport = TransportKind::TCP;
          ep.health = Health::HEALTHY; ep.reachability = Reachability::REACHABLE; ep.freshness = Freshness::CURRENT;
          ep.provenance = make_provenance(EvidenceKind::MEASURED,"p",0);
          EndpointId eid = reg.register_endpoint(ep, env);
          ServiceInstance inst; inst.service_id = sid; inst.service_generation = ServiceGeneration(1);
          inst.instance_generation = ServiceInstanceGeneration(t*per + i + 1); inst.node = NodeId(1); inst.worker = WorkerId(1);
          inst.boot = b; inst.runtime_id = rt.runtime_id; inst.runtime_instance = rt.runtime_instance; inst.endpoints = {eid};
          inst.protocols = {ProtocolId(1)}; inst.health = Health::HEALTHY; inst.readiness = Readiness::READY;
          inst.freshness = Freshness::CURRENT; inst.reachability = Reachability::REACHABLE;
          inst.version = SemanticVersion::parse("1.0.0").value();
          inst.provenance = make_provenance(EvidenceKind::REPORTED,"w",0); inst.lifecycle = Lifecycle::REGISTERING;
          reg.register_instance(inst, env);
        }
      } catch (...) { ++thread_failures; }
    });
  }
  for (auto& th : writers) th.join();
  CHECK(thread_failures.load() == 0);

  // concurrent exact lookups
  RegistryQuery rq; rq.service_kind = ServiceKind::OBSERVABILITY_SERVICE;
  std::atomic<int> read_failures{0};
  std::vector<std::thread> readers;
  for (int t=0; t<4; ++t) readers.emplace_back([&]() {
    try { for (int i=0;i<300;++i) { RegistryResult r = reg.query(rq); (void)r; } }
    catch (...) { ++read_failures; }
  });
  for (auto& th : readers) th.join();
  CHECK(read_failures.load() == 0);

  // invariants + accounting hold after concurrent mutation
  CHECK(reg.check_invariants().empty());
  CHECK(!reg.accounting_negative());
  CHECK(reg.accounting().service_instances == N);
}

int main() {
  rr_test::tests().push_back({"concurrent_registration_and_lookup", [](){ test_concurrent_registration_and_lookup(); }});
  return rr_test::run_all("concurrency");
}
