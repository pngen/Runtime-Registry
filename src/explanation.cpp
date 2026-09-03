#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/explanation.hpp>

#include <sstream>
#include <string>

namespace runtimeregistry {

std::string render_health(Health h) noexcept { return std::string(to_string(h)); }
std::string render_readiness(Readiness r) noexcept { return std::string(to_string(r)); }

std::string Registry::explain_query(const RegistryQuery& q) const {
  RegistryResult r = query(q);
  std::ostringstream os;
  os << "query outcome " << to_string(r.outcome);
  return os.str();
}
std::string Registry::explain_candidate(ServiceInstanceId id) const {
  auto it = instances_.find(id);
  if (it == instances_.end()) return "unknown instance";
  const ServiceInstance& i = it->second;
  std::ostringstream os;
  os << "ServiceInstanceId " << id.value() << " health=" << to_string(i.health)
     << " readiness=" << to_string(i.readiness)
     << " reachability=" << to_string(i.reachability)
     << " freshness=" << to_string(i.freshness)
     << " lifecycle=" << to_string(i.lifecycle);
  return os.str();
}
std::string Registry::explain_rejection(ServiceInstanceId id, const RegistryQuery& q) const {
  RegistryResult r = query(q);
  for (const auto& rej : r.rejected) if (rej.instance_id == id) return rej.reason + ": " + rej.detail;
  return "instance not rejected by query";
}
std::string Registry::explain_service(ServiceId id) const {
  auto it = services_.find(id);
  if (it == services_.end()) return "unknown service";
  const ServiceDescriptor& s = it->second;
  std::ostringstream os;
  os << "ServiceId " << id.value() << " kind=" << to_string(s.kind)
     << " api=" << s.api_version.str() << " abi=" << s.abi_version.str();
  return os.str();
}
std::string Registry::explain_runtime(RuntimeId id) const {
  auto it = runtimes_.find(id);
  if (it == runtimes_.end()) return "unknown runtime";
  const RuntimeDescriptor& r = it->second;
  std::ostringstream os;
  os << "RuntimeId " << id.value() << " kind=" << to_string(r.kind)
     << " family=" << r.family << " version=" << r.version.str();
  return os.str();
}
std::string Registry::explain_endpoint(EndpointId id) const {
  auto it = endpoints_.find(id);
  if (it == endpoints_.end()) return "unknown endpoint";
  const EndpointDescriptor& e = it->second;
  std::ostringstream os;
  os << "EndpointId " << id.value() << " transport=" << to_string(e.transport)
     << " locator=" << e.locator.text << " reachability=" << to_string(e.reachability);
  return os.str();
}
std::string Registry::explain_capability(CapabilityId id) const {
  auto it = capabilities_.find(id);
  if (it == capabilities_.end()) return "unknown capability";
  const CapabilityDescriptor& c = it->second;
  std::ostringstream os;
  os << "CapabilityId " << id.value() << " kind=" << to_string(c.kind)
     << " freshness=" << to_string(c.freshness) << " value=" << c.value.render();
  return os.str();
}
std::string Registry::explain_version(const RegistryQuery& q, ServiceInstanceId id) const {
  (void)q;
  auto it = instances_.find(id);
  if (it == instances_.end()) return "unknown instance";
  const ServiceInstance& i = it->second;
  std::ostringstream os;
  os << "ServiceInstanceId " << id.value() << " version=" << i.version.str();
  return os.str();
}
std::string Registry::explain_protocol(ServiceInstanceId id, ProtocolId p) const {
  std::ostringstream os;
  os << "ServiceInstanceId " << id.value() << " protocol " << p.value();
  return os.str();
}
std::string Registry::explain_readiness(ServiceInstanceId id) const {
  auto it = instances_.find(id);
  if (it == instances_.end()) return "unknown instance";
  std::ostringstream os;
  os << "ServiceInstanceId " << id.value() << " readiness=" << to_string(it->second.readiness);
  return os.str();
}
std::string Registry::explain_reachability(ServiceInstanceId id) const {
  auto it = instances_.find(id);
  if (it == instances_.end()) return "unknown instance";
  std::ostringstream os;
  os << "ServiceInstanceId " << id.value() << " reachability=" << to_string(it->second.reachability);
  return os.str();
}
std::string Registry::explain_lease(LeaseId id) const {
  auto it = leases_.find(id);
  if (it == leases_.end()) return "unknown lease";
  std::ostringstream os;
  os << "LeaseId " << id.value() << " state=" << to_string(it->second.state);
  return os.str();
}
std::string Registry::explain_invalidation(ServiceInstanceId id) const {
  std::ostringstream os;
  os << "ServiceInstanceId " << id.value() << " invalidation recorded";
  return os.str();
}
std::string Registry::explain_tombstone(const std::string& target_kind, const std::string& target_text) const {
  std::ostringstream os;
  os << target_kind << " " << target_text << " covered by tombstone";
  return os.str();
}
std::string Registry::explain_recovery() const {
  std::ostringstream os;
  os << "Recovered registry requires revalidation of live-process, process-local, and dynamic physical observations.";
  return os.str();
}

}  // namespace runtimeregistry
