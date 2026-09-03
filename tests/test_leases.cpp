#include <runtimeregistry/registry.hpp>
#include "test_util.hpp"
using namespace runtimeregistry;

namespace {
LeaseId make_lease(Registry& r, WorkerBootId boot, const AuthorityEnvelope& env) {
  LeaseDescriptor l; l.boot = boot; l.state = LeaseState::ACTIVE; l.renewal_policy = LeaseRenewalPolicy::BOOT;
  l.provenance = make_provenance(EvidenceKind::REPORTED, "w", 0);
  return r.acquire_lease(l, env);
}
void test_lease_lifecycle() {
  Registry r; r.begin_coordinator_epoch();
  WorkerBootId b = r.adopt_worker(WorkerId(1));
  AuthorityEnvelope env; env.epoch = r.authority().epoch; env.boot = b;
  LeaseId id = make_lease(r, b, env);
  CHECK(static_cast<bool>(id));
  const LeaseDescriptor* ld = r.find_lease(id);
  CHECK(ld && ld->state == LeaseState::ACTIVE);
  // renew
  env.lease_generation = ld->generation;
  r.renew_lease(id, env);
  CHECK(r.find_lease(id)->state == LeaseState::ACTIVE);
  // stale boot cannot renew
  AuthorityEnvelope env2; env2.epoch = r.authority().epoch; env2.boot = WorkerBootId(9999);
  CHECK_THROWS(r.renew_lease(id, env2), RegistryError, ErrorKind::STALE_BOOT);
  // stale lease generation renewal rejected
  env2.boot = b; env2.lease_generation = LeaseGeneration(999);
  CHECK_THROWS(r.renew_lease(id, env2), RegistryError, ErrorKind::STALE_LEASE_GENERATION);
  // expire
  AuthorityEnvelope env3; env3.epoch = r.authority().epoch; env3.boot = b;
  r.expire_lease(id, env3);
  CHECK(r.find_lease(id)->state == LeaseState::EXPIRED);
}
void test_lease_stale_renewal_after_restart() {
  Registry r; r.begin_coordinator_epoch();
  WorkerBootId b1 = r.adopt_worker(WorkerId(1));
  AuthorityEnvelope env; env.epoch = r.authority().epoch; env.boot = b1;
  LeaseId id = make_lease(r, b1, env);
  // worker restarts -> new boot, old boot revoked
  WorkerBootId b2 = r.adopt_worker(WorkerId(1));
  CHECK(b2 != b1);
  CHECK(!r.authority().is_boot_live(b1));
  CHECK(r.authority().is_boot_live(b2));
  // old boot cannot renew the lease owned by it (boot no longer live)
  AuthorityEnvelope envOld; envOld.epoch = r.authority().epoch; envOld.boot = b1;
  CHECK_THROWS(r.renew_lease(id, envOld), RegistryError, ErrorKind::STALE_BOOT);
}
}  // namespace

void test_leases_suite();
void test_leases_suite() { test_lease_lifecycle(); test_lease_stale_renewal_after_restart(); }
RR_REGISTER(test_leases_suite);
int main() { return rr_test::run_all("leases"); }
