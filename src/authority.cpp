#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/enums.hpp>

#include <string>

namespace runtimeregistry {

void Registry::begin_coordinator_epoch() {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  authority_.epoch = CoordinatorEpoch{authority_.epoch.value() + 1};
  authority_.live_boots.clear();
  authority_.current_boot.clear();
  authority_.boot_worker.clear();

  for (auto& [id, inst] : instances_) {
    if (inst.lifecycle == Lifecycle::AVAILABLE ||
        inst.lifecycle == Lifecycle::DEGRADED ||
        inst.lifecycle == Lifecycle::UNREADY) {
      inst.freshness = Freshness::REVALIDATION_REQUIRED;
      inst.reachability = Reachability::REVALIDATION_REQUIRED;
      if (inst.lifecycle == Lifecycle::AVAILABLE ||
          inst.lifecycle == Lifecycle::DEGRADED) {
        inst.lifecycle = Lifecycle::UNREADY;
      }
    }
    inst.updated_ms = 0;
  }

  for (auto& [id, ep] : endpoints_) {
    if (ep.transport == TransportKind::IN_PROCESS ||
        ep.transport == TransportKind::LOCAL_IPC) {
      ep.freshness = Freshness::REVALIDATION_REQUIRED;
      ep.reachability = Reachability::REVALIDATION_REQUIRED;
    } else if (ep.transport == TransportKind::TCP) {
      ep.reachability = Reachability::REVALIDATION_REQUIRED;
    }
  }

  for (auto& [id, cap] : capabilities_) {
    if (cap.freshness == Freshness::CURRENT && cap.provenance.is_physical()) {
      cap.freshness = Freshness::REVALIDATION_REQUIRED;
    }
  }

  for (auto& [id, lease] : leases_) {
    if (lease.state == LeaseState::ACTIVE) {
      lease.state = LeaseState::REVALIDATION_REQUIRED;
    }
  }
}

WorkerBootId Registry::adopt_worker(WorkerId worker) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  WorkerBootId new_boot{++next_boot_};
  auto prev = authority_.current_boot.find(worker);
  if (prev != authority_.current_boot.end() && prev->second != new_boot) {
    revoke_boot_internal(prev->second);
  }
  authority_.current_boot[worker] = new_boot;
  authority_.live_boots.insert(new_boot);
  authority_.boot_worker[new_boot] = worker;
  ++accounting_.worker_restarts;
  return new_boot;
}

void Registry::mark_worker_died(WorkerBootId boot) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  revoke_boot_internal(boot);
}

void Registry::revoke_boot_internal(WorkerBootId boot) {
  if (authority_.live_boots.erase(boot) == 0) return;
  WorkerId owner = authority_.boot_owner(boot);
  auto cur = authority_.current_boot.find(owner);
  if (cur != authority_.current_boot.end() && cur->second == boot) {
    authority_.current_boot[owner] = WorkerBootId{};
  }
  authority_.boot_worker.erase(boot);

  for (auto& [id, inst] : instances_) {
    if (inst.boot == boot) {
      if (static_cast<bool>(inst.lease)) {
        auto lit = leases_.find(inst.lease);
        if (lit != leases_.end() && lit->second.state == LeaseState::ACTIVE) {
          lit->second.state = LeaseState::EXPIRED;
          ++accounting_.expired_leases;
          if (accounting_.active_leases > 0) --accounting_.active_leases;
        }
      }
      inst.lifecycle = Lifecycle::STALE;
      inst.freshness = Freshness::STALE;
      inst.reachability = Reachability::UNREACHABLE;
      inst.readiness = Readiness::NOT_READY;
      // Release the endpoints of the dead incarnation so a fresh incarnation
      // can claim them. The endpoint's liveness died with the process, but the
      // logical endpoint identity may be re-established by a new boot.
      for (EndpointId eid : inst.endpoints) {
        auto eit = endpoints_.find(eid);
        if (eit != endpoints_.end() && eit->second.service_instance == inst.instance_id) {
          eit->second.service_instance = ServiceInstanceId{};
          eit->second.reachability = Reachability::UNREACHABLE;
        }
      }
    }
  }
}

AuthorityVerdict Registry::validate_envelope(const AuthorityEnvelope& env) const {
  if (env.epoch != authority_.epoch) {
    return {false, ErrorKind::STALE_EPOCH,
            "epoch " + std::to_string(env.epoch.value()) +
                " is stale; current epoch " +
                std::to_string(authority_.epoch.value())};
  }
  if (!authority_.is_boot_live(env.boot)) {
    return {false, ErrorKind::STALE_BOOT,
            "boot " + std::to_string(env.boot.value()) +
                " is not a live incarnation"};
  }
  return {true, ErrorKind::INTERNAL, ""};
}

void Registry::set_worker_health_reporting(WorkerId, bool) {
  // Reserved for future health-agent integration; no state change.
}

}  // namespace runtimeregistry
