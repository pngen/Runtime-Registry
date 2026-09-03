#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/crc32.hpp>
#include <runtimeregistry/selection.hpp>
#include <runtimeregistry/sha256.hpp>

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace runtimeregistry {

namespace {

std::string render_id(std::uint64_t v) { std::ostringstream os; os << v; return os.str(); }
std::string render_id(WorkerBootId id) { std::ostringstream os; os << id.value(); return os.str(); }
template <typename T> std::string render_typed_id(const T& id) { std::ostringstream os; os << id.value(); return os.str(); }

Health aggregate_health(const ServiceInstance& inst,
                        const std::unordered_map<EndpointId, EndpointDescriptor>& eps) noexcept {
  Health h = inst.health;
  for (EndpointId eid : inst.endpoints) {
    auto it = eps.find(eid);
    if (it == eps.end()) continue;
    if (it->second.health == Health::UNHEALTHY || it->second.health == Health::UNAVAILABLE) h = Health::UNHEALTHY;
    else if (it->second.health == Health::DEGRADED && h != Health::UNHEALTHY) h = Health::DEGRADED;
  }
  return h;
}

Reachability aggregate_reachability(const ServiceInstance& inst,
    const std::unordered_map<EndpointId, EndpointDescriptor>& eps) noexcept {
  if (inst.endpoints.empty()) return inst.reachability;
  bool degraded = false;
  for (EndpointId eid : inst.endpoints) {
    auto it = eps.find(eid);
    if (it == eps.end()) return Reachability::UNKNOWN;
    if (it->second.reachability == Reachability::UNREACHABLE) return Reachability::UNREACHABLE;
    if (it->second.reachability == Reachability::DEGRADED) degraded = true;
    if (it->second.reachability == Reachability::REVALIDATION_REQUIRED || it->second.reachability == Reachability::UNKNOWN) return Reachability::REVALIDATION_REQUIRED;
  }
  return degraded ? Reachability::DEGRADED : Reachability::REACHABLE;
}

bool capability_current(const CapabilityDescriptor& cap) noexcept { return cap.freshness == Freshness::CURRENT; }

}  // namespace

Registry::Registry() : Registry(RegistryOptions{}) {}

Registry::Registry(RegistryOptions opts) : options_(opts) {
  authority_.epoch = CoordinatorEpoch{1};
  authority_.registry_generation = RegistryGeneration{1};
  authority_.record_generation = RecordGeneration{1};
  next_boot_ = 100;
  next_id_ = 1;
  rebuild_indexes();
}

void Registry::commit_registry_generation() {
  authority_.registry_generation = RegistryGeneration{authority_.registry_generation.value() + 1};
  authority_.record_generation = RecordGeneration{authority_.record_generation.value() + 1};
  authority_.authority_gen = AuthorityGeneration{authority_.authority_gen.value() + 1};
}

void Registry::register_service(const ServiceDescriptor& svc, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = services_.find(svc.service_id);
  if (it != services_.end()) {
    if (svc.generation == it->second.generation && svc == it->second) return;  // idempotent
    if (svc.generation <= it->second.generation) {
      ++accounting_.duplicate_conflict_rejections;
      throw RegistryError(ErrorKind::STALE_SERVICE_GENERATION,
          "registration rejected because ServiceGeneration " + std::to_string(svc.generation.value()) + " is stale; current " + std::to_string(it->second.generation.value()));
    }
    it->second = svc;
  } else {
    if (covered_by_tombstone("ServiceId", render_typed_id(svc.service_id), svc.generation.value())) {
      ++accounting_.stale_mutation_rejections;
      throw RegistryError(ErrorKind::TOMBSTONE_RESURRECTION, "registration rejected because ServiceId is covered by a tombstone");
    }
    services_[svc.service_id] = svc;
    ++accounting_.services;
  }
  if (authority_.service_gen < svc.generation) authority_.service_gen = svc.generation;
  commit_registry_generation();
  rebuild_indexes();
}

RuntimeInstanceId Registry::register_runtime(const RuntimeDescriptor& rt, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  RuntimeDescriptor copy = rt;
  auto it = runtime_instances_.find(rt.runtime_instance);
  if (it != runtime_instances_.end()) {
    if (rt.generation == it->second.generation && rt == it->second) return rt.runtime_instance;
    if (rt.generation <= it->second.generation) {
      ++accounting_.duplicate_conflict_rejections;
      throw RegistryError(ErrorKind::STALE_RUNTIME_GENERATION, "stale RuntimeGeneration " + std::to_string(rt.generation.value()));
    }
    it->second = copy;
  } else {
    if (covered_by_tombstone("RuntimeInstanceId", render_typed_id(rt.runtime_instance), rt.generation.value())) {
      ++accounting_.stale_mutation_rejections;
      throw RegistryError(ErrorKind::TOMBSTONE_RESURRECTION, "runtime instance resurrected by tombstone");
    }
    if (rt.runtime_instance.value() == 0) copy.runtime_instance = RuntimeInstanceId{next_id_++};
    runtimes_[rt.runtime_id] = copy;
    runtime_instances_[copy.runtime_instance] = copy;
    runtime_history_.push_back(copy);
    ++accounting_.runtimes;
    ++accounting_.runtime_instances;
  }
  if (authority_.runtime_gen < copy.generation) authority_.runtime_gen = copy.generation;
  commit_registry_generation();
  return copy.runtime_instance;
}

void Registry::register_protocol(const ProtocolDescriptor& pd, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = protocols_.find(pd.protocol_id);
  if (it != protocols_.end()) {
    if (pd.generation == it->second.generation && pd == it->second) return;
    if (pd.generation <= it->second.generation) {
      ++accounting_.duplicate_conflict_rejections;
      throw RegistryError(ErrorKind::DUPLICATE_CONFLICT, "stale or duplicate ProtocolId registration");
    }
    it->second = pd;
  } else {
    protocols_[pd.protocol_id] = pd;
    ++accounting_.protocols;
  }
  if (authority_.protocol_gen < pd.generation) authority_.protocol_gen = pd.generation;
  commit_registry_generation();
}

EndpointId Registry::register_endpoint(const EndpointDescriptor& ep, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  EndpointDescriptor copy = ep;
  auto it = endpoints_.find(ep.endpoint_id);
  if (it != endpoints_.end()) {
    if (ep.generation == it->second.generation && ep == it->second) return ep.endpoint_id;
    if (ep.generation <= it->second.generation) {
      ++accounting_.duplicate_conflict_rejections;
      throw RegistryError(ErrorKind::STALE_ENDPOINT_GENERATION, "stale EndpointGeneration " + std::to_string(ep.generation.value()));
    }
    endpoint_history_.push_back(it->second);
    it->second = copy;
    ++accounting_.supersessions;
  } else {
    if (covered_by_tombstone("EndpointId", render_typed_id(ep.endpoint_id), ep.generation.value())) {
      ++accounting_.stale_mutation_rejections;
      throw RegistryError(ErrorKind::TOMBSTONE_RESURRECTION, "endpoint resurrected by tombstone");
    }
    if (copy.endpoint_id.value() == 0) copy.endpoint_id = EndpointId{next_id_++};
    endpoints_[copy.endpoint_id] = copy;
    endpoint_history_.push_back(copy);
    ++accounting_.endpoints;
  }
  if (authority_.endpoint_gen < copy.generation) authority_.endpoint_gen = copy.generation;
  commit_registry_generation();
  return copy.endpoint_id;
}

CapabilityId Registry::register_capability(const CapabilityDescriptor& cap, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  CapabilityDescriptor copy = cap;
  auto it = capabilities_.find(cap.capability_id);
  if (it != capabilities_.end()) {
    if (cap.generation == it->second.generation && cap == it->second) return cap.capability_id;
    if (cap.generation <= it->second.generation) {
      ++accounting_.duplicate_conflict_rejections;
      throw RegistryError(ErrorKind::STALE_CAPABILITY_GENERATION, "stale CapabilityGeneration " + std::to_string(cap.generation.value()));
    }
    capability_history_.push_back(it->second);
    it->second = copy;
    ++accounting_.supersessions;
  } else {
    if (covered_by_tombstone("CapabilityId", render_typed_id(cap.capability_id), cap.generation.value())) {
      ++accounting_.stale_mutation_rejections;
      throw RegistryError(ErrorKind::TOMBSTONE_RESURRECTION, "capability resurrected by tombstone");
    }
    if (copy.capability_id.value() == 0) copy.capability_id = CapabilityId{next_id_++};
    capabilities_[copy.capability_id] = copy;
    capability_history_.push_back(copy);
    ++accounting_.capabilities;
  }
  if (authority_.capability_gen < copy.generation) authority_.capability_gen = copy.generation;
  commit_registry_generation();
  return copy.capability_id;
}

BackendId Registry::register_backend(const BackendDescriptor& bd, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  BackendDescriptor copy = bd;
  auto it = backends_.find(bd.backend_id);
  if (it != backends_.end()) {
    if (bd.generation == it->second.generation && bd == it->second) return bd.backend_id;
    if (bd.generation <= it->second.generation) { ++accounting_.duplicate_conflict_rejections; throw RegistryError(ErrorKind::STALE_EPOCH, "stale BackendGeneration"); }
    it->second = copy;
  } else {
    if (copy.backend_id.value() == 0) copy.backend_id = BackendId{next_id_++};
    backends_[copy.backend_id] = copy;
    ++accounting_.backends;
  }
  if (authority_.backend_gen < copy.generation) authority_.backend_gen = copy.generation;
  commit_registry_generation();
  return copy.backend_id;
}

DeviceId Registry::register_device(const DeviceDescriptor& dd, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  DeviceDescriptor copy = dd;
  auto it = devices_.find(dd.device_id);
  if (it != devices_.end()) {
    if (dd.generation == it->second.generation && dd == it->second) return dd.device_id;
    if (dd.generation <= it->second.generation) { ++accounting_.duplicate_conflict_rejections; throw RegistryError(ErrorKind::STALE_EPOCH, "stale DeviceGeneration"); }
    it->second = copy;
  } else {
    if (copy.device_id.value() == 0) copy.device_id = DeviceId{next_id_++};
    devices_[copy.device_id] = copy;
    ++accounting_.devices;
  }
  if (authority_.device_gen < copy.generation) authority_.device_gen = copy.generation;
  commit_registry_generation();
  return copy.device_id;
}

void Registry::register_node(const NodeDescriptor& nd, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = nodes_.find(nd.node_id);
  if (it != nodes_.end()) {
    if (nd.generation == it->second.generation && nd == it->second) return;
    if (nd.generation <= it->second.generation) { ++accounting_.duplicate_conflict_rejections; throw RegistryError(ErrorKind::STALE_EPOCH, "stale NodeGeneration"); }
    it->second = nd;
  } else { nodes_[nd.node_id] = nd; ++accounting_.nodes; }
  if (authority_.node_gen < nd.generation) authority_.node_gen = nd.generation;
  commit_registry_generation();
}

ServiceInstanceId Registry::register_instance(const ServiceInstance& inst, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  if (services_.find(inst.service_id) == services_.end())
    throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "instance references unregistered ServiceId");
  ServiceInstance copy = inst;
  if (copy.instance_id.value() == 0) copy.instance_id = ServiceInstanceId{next_id_++};
  ServiceInstance* current = nullptr;
  for (auto& [id, existing] : instances_) {
    if (existing.service_id == copy.service_id && existing.lifecycle != Lifecycle::SUPERSEDED &&
        existing.lifecycle != Lifecycle::TOMBSTONED && existing.lifecycle != Lifecycle::RETIRED &&
        existing.lifecycle != Lifecycle::INVALIDATED) {
      if (current == nullptr || existing.instance_generation > current->instance_generation) current = &existing;
    }
  }
  if (current != nullptr) {
    if (copy.instance_generation == current->instance_generation && copy == *current) return copy.instance_id;
    if (copy.instance_generation <= current->instance_generation) {
      ++accounting_.duplicate_conflict_rejections;
      throw RegistryError(ErrorKind::STALE_INSTANCE_GENERATION, "registration rejected because ServiceInstanceGeneration " + std::to_string(copy.instance_generation.value()) + " is stale; current " + std::to_string(current->instance_generation.value()));
    }
    instance_history_.push_back(*current);
    current->lifecycle = Lifecycle::SUPERSEDED;
    if (accounting_.available_instances > 0) --accounting_.available_instances;
    ++accounting_.supersessions;
  } else {
    if (covered_by_tombstone("ServiceId", render_typed_id(copy.service_id), copy.instance_generation.value())) {
      ++accounting_.stale_mutation_rejections;
      throw RegistryError(ErrorKind::TOMBSTONE_RESURRECTION, "instance resurrected by tombstone");
    }
  }
  for (EndpointId eid : copy.endpoints) {
    auto eit = endpoints_.find(eid);
    if (eit == endpoints_.end()) throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "instance references unregistered endpoint");
    if (eit->second.service_instance.value() == 0) {
      eit->second.service_instance = copy.instance_id;
    } else if (eit->second.service_instance != copy.instance_id) {
      // An inactive (superseded/stale/tombstoned/retired) instance may have its
      // endpoint reclaimed by a fresh incarnation of the same service.
      auto owner = instances_.find(eit->second.service_instance);
      bool inactive = (owner == instances_.end() || owner->second.lifecycle == Lifecycle::SUPERSEDED ||
                       owner->second.lifecycle == Lifecycle::STALE || owner->second.lifecycle == Lifecycle::TOMBSTONED ||
                       owner->second.lifecycle == Lifecycle::RETIRED || owner->second.lifecycle == Lifecycle::INVALIDATED);
      if (!inactive) { ++accounting_.duplicate_conflict_rejections; throw RegistryError(ErrorKind::DUPLICATE_CONFLICT, "endpoint already bound to another instance"); }
      eit->second.service_instance = copy.instance_id;
      eit->second.reachability = Reachability::REACHABLE;
      eit->second.freshness = Freshness::CURRENT;
    }
  }
  instances_[copy.instance_id] = copy;
  instance_history_.push_back(copy);
  ++accounting_.service_instances;
  ++accounting_.registrations;
  reclassify_lifecycle(instances_[copy.instance_id]);
  commit_registry_generation();
  rebuild_indexes();
  return copy.instance_id;
}

void Registry::update_health(ServiceInstanceId id, Health h, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = instances_.find(id);
  if (it == instances_.end()) throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "no such instance");
  if (!generation_is_current_generation(env, it->second)) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_HEALTH, "stale health update; instance was superseded"); }
  it->second.health = h; reclassify_lifecycle(it->second); commit_registry_generation();
}

void Registry::update_readiness(ServiceInstanceId id, Readiness r, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = instances_.find(id);
  if (it == instances_.end()) throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "no such instance");
  if (!generation_is_current_generation(env, it->second)) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_READINESS, "stale readiness update; instance was superseded"); }
  it->second.readiness = r; reclassify_lifecycle(it->second); commit_registry_generation();
}

void Registry::update_freshness(ServiceInstanceId id, Freshness f, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = instances_.find(id);
  if (it == instances_.end()) throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "no such instance");
  if (!generation_is_current_generation(env, it->second)) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_EPOCH, "stale freshness update"); }
  it->second.freshness = f; reclassify_lifecycle(it->second); commit_registry_generation();
}

void Registry::update_reachability(EndpointId id, Reachability r, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = endpoints_.find(id);
  if (it == endpoints_.end()) throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "no such endpoint");
  if (env.endpoint_generation.value() != 0 && env.endpoint_generation != it->second.generation) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_EPOCH, "stale endpoint generation"); }
  it->second.reachability = r;
  if (static_cast<bool>(it->second.service_instance)) { auto iit = instances_.find(it->second.service_instance); if (iit != instances_.end()) reclassify_lifecycle(iit->second); }
  commit_registry_generation();
}

void Registry::update_instance_reachability(ServiceInstanceId id, Reachability r, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = instances_.find(id);
  if (it == instances_.end()) throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "no such instance");
  if (!generation_is_current_generation(env, it->second)) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_EPOCH, "stale reachability update"); }
  it->second.reachability = r; reclassify_lifecycle(it->second); commit_registry_generation();
}

bool Registry::generation_is_current_generation(const AuthorityEnvelope& env, const ServiceInstance& inst) const {
  if (env.service_instance_generation.value() != 0 && env.service_instance_generation != inst.instance_generation) return false;
  return true;
}

void Registry::reclassify_lifecycle(ServiceInstance& inst) {
  Health h = aggregate_health(inst, endpoints_);
  Reachability reach = aggregate_reachability(inst, endpoints_);
  inst.health = h;
  inst.reachability = reach;
  if (!authority_.is_boot_live(inst.boot)) { inst.lifecycle = Lifecycle::STALE; return; }
  if (inst.freshness != Freshness::CURRENT) { inst.lifecycle = Lifecycle::STALE; return; }
  if (reach == Reachability::UNREACHABLE) { inst.lifecycle = Lifecycle::UNREACHABLE; return; }
  if (reach == Reachability::REVALIDATION_REQUIRED || reach == Reachability::UNKNOWN) { inst.lifecycle = Lifecycle::UNREADY; return; }
  if (static_cast<bool>(inst.lease)) {
    auto lit = leases_.find(inst.lease);
    if (lit == leases_.end() || lit->second.state != LeaseState::ACTIVE) { inst.lifecycle = Lifecycle::STALE; return; }
  }
  auto svc = services_.find(inst.service_id);
  if (svc != services_.end()) {
    for (CapabilityKind req : svc->second.required_capabilities) {
      bool found = false;
      for (CapabilityId cid : inst.capabilities) {
        auto cit = capabilities_.find(cid);
        if (cit == capabilities_.end()) continue;
        if (cit->second.kind == req && capability_current(cit->second)) { found = true; break; }
      }
      if (!found) { inst.lifecycle = Lifecycle::UNREADY; return; }
    }
  }
  if (inst.readiness == Readiness::DRAINING) { inst.lifecycle = Lifecycle::DRAINING; return; }
  if (inst.readiness == Readiness::NOT_READY || inst.readiness == Readiness::REVALIDATION_REQUIRED || inst.readiness == Readiness::UNKNOWN) { inst.lifecycle = Lifecycle::UNREADY; return; }
  if (h == Health::UNHEALTHY || h == Health::UNAVAILABLE) { inst.lifecycle = Lifecycle::UNREADY; return; }
  if (h == Health::DEGRADED || inst.readiness == Readiness::PARTIALLY_READY || reach == Reachability::DEGRADED) { inst.lifecycle = Lifecycle::DEGRADED; return; }
  inst.lifecycle = Lifecycle::AVAILABLE;
}

LeaseId Registry::acquire_lease(const LeaseDescriptor& lease, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  LeaseDescriptor copy = lease;
  if (copy.lease_id.value() == 0) copy.lease_id = LeaseId{next_id_++};
  copy.generation = LeaseGeneration{authority_.lease_gen.value() + 1};
  authority_.lease_gen = copy.generation;
  leases_[copy.lease_id] = copy;
  if (copy.state == LeaseState::ACTIVE) ++accounting_.active_leases;
  ++accounting_.leases;
  commit_registry_generation();
  return copy.lease_id;
}

void Registry::renew_lease(LeaseId id, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = leases_.find(id);
  if (it == leases_.end()) throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "no such lease");
  if (it->second.state == LeaseState::REVOKED) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_RENEWAL, "lease revoked"); }
  auto iit = instances_.find(it->second.service_instance);
  if (iit != instances_.end() && iit->second.boot != env.boot) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_BOOT, "stale boot cannot renew current lease"); }
  if (env.lease_generation.value() != 0 && env.lease_generation != it->second.generation) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_LEASE_GENERATION, "stale lease generation renewal"); }
  it->second.state = LeaseState::ACTIVE;
  commit_registry_generation();
}

void Registry::expire_lease(LeaseId id, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = leases_.find(id);
  if (it == leases_.end()) return;
  if (it->second.state == LeaseState::ACTIVE) { it->second.state = LeaseState::EXPIRED; ++accounting_.expired_leases; if (accounting_.active_leases > 0) --accounting_.active_leases; }
  commit_registry_generation();
}

void Registry::revoke_lease(LeaseId id, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = leases_.find(id);
  if (it == leases_.end()) return;
  if (it->second.state == LeaseState::ACTIVE) { it->second.state = LeaseState::REVOKED; ++accounting_.expired_leases; if (accounting_.active_leases > 0) --accounting_.active_leases; }
  commit_registry_generation();
}

void Registry::mark_lease_revalidation_required(LeaseId id) {
  auto it = leases_.find(id);
  if (it == leases_.end()) return;
  if (it->second.state == LeaseState::ACTIVE) { it->second.state = LeaseState::REVALIDATION_REQUIRED; if (accounting_.active_leases > 0) --accounting_.active_leases; }
}

void Registry::invalidate(const InvalidationRecord& rec, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  invalidations_.push_back(rec);
  ++accounting_.invalidations;
  commit_registry_generation();
}

void Registry::supersede(ServiceInstanceId old_id, const ServiceInstance& new_inst, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = instances_.find(old_id);
  if (it == instances_.end()) throw RegistryError(ErrorKind::UNKNOWN_IDENTITY, "no such instance");
  instance_history_.push_back(it->second);
  it->second.lifecycle = Lifecycle::SUPERSEDED;
  if (accounting_.available_instances > 0) --accounting_.available_instances;
  register_instance(new_inst, env);
  ++accounting_.supersessions;
}

TombstoneId Registry::create_tombstone(const TombstoneRecord& tomb, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  TombstoneRecord copy = tomb;
  if (copy.tombstone_id.value() == 0) copy.tombstone_id = TombstoneId{next_id_++};
  tombstones_[copy.tombstone_id] = copy;
  ++accounting_.tombstones;
  commit_registry_generation();
  return copy.tombstone_id;
}

void Registry::deregister(ServiceInstanceId id, const AuthorityEnvelope& env) {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  AuthorityVerdict v = validate_envelope(env);
  if (!v.accept) throw RegistryError(v.kind, v.reason);
  auto it = instances_.find(id);
  if (it == instances_.end()) return;
  if (!generation_is_current_generation(env, it->second)) { ++accounting_.stale_mutation_rejections; throw RegistryError(ErrorKind::STALE_INSTANCE_GENERATION, "stale deregister"); }
  instance_history_.push_back(it->second);
  it->second.lifecycle = Lifecycle::RETIRED;
  if (accounting_.available_instances > 0) --accounting_.available_instances;
  ++accounting_.deregistrations;
  commit_registry_generation();
}

bool Registry::covered_by_tombstone(const std::string& target_kind, const std::string& target_text, std::uint64_t gen_floor) const {
  for (const auto& kv : tombstones_) {
    const TombstoneRecord& tomb = kv.second;
    if (tomb.target_kind == target_kind && tomb.target_text == target_text && gen_floor <= tomb.generation_floor.value()) return true;
  }
  return false;
}

void Registry::rebuild_indexes() {
  index_by_kind_.clear(); index_by_service_.clear(); index_by_boot_.clear(); index_by_node_.clear();
  for (const auto& kv : instances_) {
    const ServiceInstance& inst = kv.second;
    index_by_service_[inst.service_id].insert(inst.instance_id);
    index_by_boot_[inst.boot].insert(inst.instance_id);
    index_by_node_[inst.node].insert(inst.instance_id);
    auto svc = services_.find(inst.service_id);
    if (svc != services_.end()) index_by_kind_[svc->second.kind].insert(inst.service_id);
  }
}

std::unordered_map<ServiceId, std::vector<ServiceInstanceId>> Registry::instance_index_by_service() const {
  std::unordered_map<ServiceId, std::vector<ServiceInstanceId>> m;
  for (const auto& kv : instances_) m[kv.second.service_id].push_back(kv.first);
  for (auto& pair : m) std::sort(pair.second.begin(), pair.second.end());
  return m;
}

std::vector<std::string> Registry::check_invariants() const {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  std::vector<std::string> violations;
  auto idx = instance_index_by_service();
  for (const auto& pair : idx) {
    ServiceId sid = pair.first;
    auto it = index_by_service_.find(sid);
    if (it == index_by_service_.end() || it->second.size() != pair.second.size())
      violations.push_back("service index disagrees with canonical records for " + render_id(sid.value()));
  }
  for (const auto& kv : instances_) {
    const ServiceInstance& inst = kv.second;
    if (services_.find(inst.service_id) == services_.end()) violations.push_back("instance references unregistered service");
    for (EndpointId eid : inst.endpoints) if (endpoints_.find(eid) == endpoints_.end()) violations.push_back("instance references unregistered endpoint");
    if (inst.lifecycle == Lifecycle::AVAILABLE && (!authority_.is_boot_live(inst.boot) || inst.freshness != Freshness::CURRENT)) violations.push_back("AVAILABLE instance has stale authority/freshness");
  }
  return violations;
}

bool Registry::accounting_negative() const {
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  const Accounting& a = accounting_;
  return a.services < 0 || a.service_instances < 0 || a.runtimes < 0 || a.runtime_instances < 0 || a.nodes < 0 || a.endpoints < 0 || a.protocols < 0 || a.backends < 0 || a.devices < 0 || a.capabilities < 0 || a.leases < 0 || a.active_leases < 0 || a.expired_leases < 0;
}

const ServiceInstance* Registry::find_instance(ServiceInstanceId id) const { auto it = instances_.find(id); return it == instances_.end() ? nullptr : &it->second; }
const ServiceDescriptor* Registry::find_service(ServiceId id) const { auto it = services_.find(id); return it == services_.end() ? nullptr : &it->second; }
const RuntimeDescriptor* Registry::find_runtime(RuntimeId id) const { auto it = runtimes_.find(id); return it == runtimes_.end() ? nullptr : &it->second; }
const RuntimeDescriptor* Registry::find_runtime_instance(RuntimeInstanceId id) const { auto it = runtime_instances_.find(id); return it == runtime_instances_.end() ? nullptr : &it->second; }
const EndpointDescriptor* Registry::find_endpoint(EndpointId id) const { auto it = endpoints_.find(id); return it == endpoints_.end() ? nullptr : &it->second; }
const CapabilityDescriptor* Registry::find_capability(CapabilityId id) const { auto it = capabilities_.find(id); return it == capabilities_.end() ? nullptr : &it->second; }
const LeaseDescriptor* Registry::find_lease(LeaseId id) const { auto it = leases_.find(id); return it == leases_.end() ? nullptr : &it->second; }

}  // namespace runtimeregistry