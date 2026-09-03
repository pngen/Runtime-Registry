#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/selection.hpp>
#include "test_util.hpp"
using namespace runtimeregistry;

namespace {

struct Q {
  Registry reg;
  WorkerBootId boot;
  AuthorityEnvelope env;
  RuntimeId runtime;
  RuntimeInstanceId rtinst;
  CapabilityId cap;
  std::vector<ServiceId> services;

  Q() {
    reg.begin_coordinator_epoch();
    boot = reg.adopt_worker(WorkerId(1));
    env.epoch = reg.authority().epoch; env.boot = boot;
    RuntimeDescriptor rt; rt.runtime_id = RuntimeId(1); rt.runtime_instance = RuntimeInstanceId(1);
    rt.generation = RuntimeGeneration(1); rt.kind = RuntimeKind::NATIVE_CPP; rt.name="rt";
    rt.family="cpp"; rt.version = SemanticVersion::parse("1.0.0").value(); rt.api_version=ApiVersion(1,0); rt.abi_version=AbiVersion(1,0);
    reg.register_runtime(rt, env);
    runtime = rt.runtime_id; rtinst = rt.runtime_instance;
    CapabilityDescriptor c; c.capability_id = CapabilityId(1); c.kind = CapabilityKind::CUDA_AVAILABLE;
    c.generation = CapabilityGeneration(1); c.version = SemanticVersion::parse("1.0.0").value();
    c.value = CapabilityValue::make_bool(true); c.freshness = Freshness::CURRENT;
    c.provenance = make_provenance(EvidenceKind::MEASURED, "dev", 0);
    reg.register_capability(c, env);
    cap = c.capability_id;
  }

  ServiceInstanceId add(NodeId node, const SemanticVersion& ver, Readiness ready, Reachability reach,
                        bool with_cap=true) {
    ServiceId sid(100 + static_cast<int>(services.size()) + 1);
    services.push_back(sid);
    ServiceDescriptor svc; svc.service_id = sid; svc.kind = ServiceKind::MODEL_SERVER; svc.name = "svc";
    svc.generation = ServiceGeneration(1); svc.version = ver;
    svc.api_version = ApiVersion(2,0); svc.abi_version = AbiVersion(1,0);
    svc.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
    reg.register_service(svc, env);
    EndpointDescriptor ep; ep.endpoint_id = EndpointId(0); ep.generation = EndpointGeneration(1);
    ep.protocol = ProtocolId(1); ep.locator.text = "127.0.0.1:3000"; ep.transport = TransportKind::TCP;
    ep.health = Health::HEALTHY; ep.reachability = reach; ep.freshness = Freshness::CURRENT;
    ep.provenance = make_provenance(EvidenceKind::MEASURED, "probe", 0);
    EndpointId eid = reg.register_endpoint(ep, env);
    ServiceInstance inst; inst.service_id = sid; inst.service_generation = ServiceGeneration(1);
    inst.instance_generation = ServiceInstanceGeneration(1); inst.node = node; inst.worker = WorkerId(1);
    inst.boot = boot; inst.runtime_id = runtime; inst.runtime_instance = rtinst; inst.endpoints = {eid};
    inst.protocols = {ProtocolId(1)}; if (with_cap) inst.capabilities = {cap};
    inst.health = Health::HEALTHY; inst.readiness = ready; inst.freshness = Freshness::CURRENT;
    inst.reachability = reach; inst.version = ver; inst.provenance = make_provenance(EvidenceKind::REPORTED, "w", 0);
    inst.lifecycle = Lifecycle::REGISTERING;
    return reg.register_instance(inst, env);
  }
};

void test_capability_aware_selection() {
  Q q;
  q.add(NodeId(1), SemanticVersion::parse("1.0.0").value(), Readiness::READY, Reachability::REACHABLE);
  q.add(NodeId(1), SemanticVersion::parse("2.0.0").value(), Readiness::READY, Reachability::REACHABLE);
  RegistryQuery rq; rq.service_kind = ServiceKind::MODEL_SERVER;
  rq.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  RegistryResult res = q.reg.query(rq);
  CHECK(res.outcome == QueryOutcome::FOUND_MULTIPLE);
  CHECK(res.ranked.size() == 2);
  // deterministic: newer version (2.0.0) ranks first by version recency
  CHECK(res.ranked[0].total >= res.ranked[1].total);
  std::uint64_t newer = 0, older = 0;
  for (const auto& cs : res.ranked) {
    const ServiceInstance* inst = q.reg.find_instance(cs.instance_id);
    if (inst && inst->version == SemanticVersion::parse("2.0.0").value()) newer = cs.instance_id.value();
    if (inst && inst->version == SemanticVersion::parse("1.0.0").value()) older = cs.instance_id.value();
  }
  CHECK(newer != 0 && older != 0);
  CHECK(res.ranked[0].instance_id == ServiceInstanceId(newer));
}

void test_unready_filtered_when_required() {
  Q q;
  q.add(NodeId(1), SemanticVersion::parse("1.0.0").value(), Readiness::READY, Reachability::REACHABLE);
  q.add(NodeId(1), SemanticVersion::parse("1.0.0").value(), Readiness::NOT_READY, Reachability::REACHABLE);
  RegistryQuery rq; rq.service_kind = ServiceKind::MODEL_SERVER;
  rq.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  rq.required_readiness = Readiness::READY;
  RegistryResult res = q.reg.query(rq);
  CHECK(res.ranked.size() == 1);
  CHECK(res.rejected.size() == 1);
}

void test_unreachable_excluded() {
  Q q;
  q.add(NodeId(1), SemanticVersion::parse("1.0.0").value(), Readiness::READY, Reachability::REACHABLE);
  q.add(NodeId(1), SemanticVersion::parse("1.0.0").value(), Readiness::READY, Reachability::UNREACHABLE);
  RegistryQuery rq; rq.service_kind = ServiceKind::MODEL_SERVER;
  rq.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  rq.required_reachability = Reachability::REACHABLE;
  RegistryResult res = q.reg.query(rq);
  CHECK(res.ranked.size() == 1);
}

void test_missing_capability_excluded() {
  Q q;
  q.add(NodeId(1), SemanticVersion::parse("1.0.0").value(), Readiness::READY, Reachability::REACHABLE);
  q.add(NodeId(3), SemanticVersion::parse("1.0.0").value(), Readiness::READY, Reachability::REACHABLE, /*with_cap=*/false);
  RegistryQuery rq; rq.service_kind = ServiceKind::MODEL_SERVER;
  rq.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  RegistryResult res = q.reg.query(rq);
  CHECK(res.ranked.size() == 1);
  bool rejected42 = false;
  for (const auto& rej : res.rejected) if (rej.category == RejectionCategory::MISSING_CAPABILITY) rejected42 = true;
  CHECK(rejected42);
}

void test_tie_break_deterministic() {
  Q q;
  q.add(NodeId(1), SemanticVersion::parse("1.0.0").value(), Readiness::READY, Reachability::REACHABLE);
  q.add(NodeId(2), SemanticVersion::parse("1.0.0").value(), Readiness::READY, Reachability::REACHABLE);
  RegistryQuery rq; rq.service_kind = ServiceKind::MODEL_SERVER;
  rq.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
  rq.required_readiness = Readiness::READY;
  RegistryResult a = q.reg.query(rq);
  RegistryResult b = q.reg.query(rq);
  CHECK(a.ranked.size() == 2);
  CHECK(a.ranked[0].instance_id == b.ranked[0].instance_id);
  CHECK(a.ranked[1].instance_id == b.ranked[1].instance_id);
}
}  // namespace

void test_query_suite();
void test_query_suite() {
  test_capability_aware_selection();
  test_unready_filtered_when_required();
  test_unreachable_excluded();
  test_missing_capability_excluded();
  test_tie_break_deterministic();
}
RR_REGISTER(test_query_suite);
int main() { return rr_test::run_all("query"); }
