#include <runtimeregistry/registry.hpp>
#include <runtimeregistry/protocol.hpp>
#include <runtimeregistry/tcp.hpp>
#include <runtimeregistry/selection.hpp>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <map>

using namespace runtimeregistry;

namespace {

void send_frame(TcpSocket& s, MessageKind kind, const std::string& payload) {
  Frame f; f.kind = kind; f.payload.assign(payload.begin(), payload.end());
  std::vector<std::uint8_t> enc = encode_frame(f);
  (void)s.send(enc.data(), enc.size());
}

std::string recv_one(TcpSocket& s) {
  std::uint8_t hdr[10];
  int n = s.recv(hdr, 10);
  if (n != 10) return {};
  std::uint32_t len = static_cast<std::uint32_t>(hdr[6]) | (static_cast<std::uint32_t>(hdr[7]) << 8) |
                      (static_cast<std::uint32_t>(hdr[8]) << 16) | (static_cast<std::uint32_t>(hdr[9]) << 24);
  if (len > kMaxPayload) return {};
  std::vector<std::uint8_t> body(10 + len + 4);  // header + payload + CRC
  std::memcpy(body.data(), hdr, 10);
  std::size_t got = 10;
  while (got < body.size()) {
    n = s.recv(body.data() + got, body.size() - got);
    if (n <= 0) return {};
    got += n;
  }
  auto dec = decode_frame(body.data(), body.size());
  if (!dec) return {};
  return std::string(reinterpret_cast<const char*>(dec->payload.data()), dec->payload.size());
}

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out; std::string cur;
  for (char c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur.push_back(c); }
  out.push_back(cur); return out;
}

void handle_connection(TcpSocket conn, Registry& reg, CoordinatorEpoch epoch, std::atomic<bool>& stop) {
  WorkerBootId boot;
  WorkerId worker;
  bool has_worker = false;
  while (!stop) {
    std::string line = recv_one(conn);
    if (line.empty()) break;
    auto p = split(line, ',');
    std::string op = p[0];
    if (op == "HELLO") {
      worker = WorkerId(std::stoull(p[1]));
      boot = reg.adopt_worker(worker);
      AuthorityEnvelope env; env.epoch = epoch; env.boot = boot;
      send_frame(conn, MessageKind::ACK, "BOOT=" + std::to_string(boot.value()));
      has_worker = true;
    } else if (op == "PING") {
      send_frame(conn, MessageKind::ACK, "PONG");
    } else if (op == "QUERY") {
      RegistryQuery q; q.service_kind = service_kind_from_string(p[1]);
      RegistryResult r = reg.query(q);
      std::string out = "RESULT_" + std::string(to_string(r.outcome));
      for (const auto& cs : r.ranked) out += "," + std::to_string(cs.instance_id.value());
      send_frame(conn, MessageKind::QUERY_RESULT, out);
    } else if (!has_worker) {
      send_frame(conn, MessageKind::ERROR, "NO_WORKER");
    } else if (op == "REG_RT") {
      RuntimeDescriptor rt; rt.runtime_id = RuntimeId(std::stoull(p[1])); rt.runtime_instance = RuntimeInstanceId(std::stoull(p[2]));
      rt.generation = RuntimeGeneration(std::stoull(p[3])); rt.kind = RuntimeKind::NATIVE_CPP; rt.name = p[4];
      rt.family = p[5] == "cuda" ? "cuda" : "cpp"; rt.version = SemanticVersion::parse("1.0.0").value();
      rt.api_version = ApiVersion(1,0); rt.abi_version = AbiVersion(1,0); rt.architecture = p[6];
      AuthorityEnvelope env; env.epoch = epoch; env.boot = boot;
      try { reg.register_runtime(rt, env); send_frame(conn, MessageKind::ACK, "OK"); }
      catch (const RegistryError& e) { send_frame(conn, MessageKind::ERROR, e.what()); }
    } else if (op == "REG_SV") {
      ServiceDescriptor svc; svc.service_id = ServiceId(std::stoull(p[1])); svc.generation = ServiceGeneration(std::stoull(p[2]));
      svc.kind = service_kind_from_string(p[3]); svc.name = p[4]; svc.version = SemanticVersion::parse("1.0.0").value();
      svc.api_version = ApiVersion(1,0); svc.abi_version = AbiVersion(1,0);
      if (p.size() > 5 && p[5] == "CUDA") svc.required_capabilities = {CapabilityKind::CUDA_AVAILABLE};
      AuthorityEnvelope env; env.epoch = epoch; env.boot = boot;
      try { reg.register_service(svc, env); send_frame(conn, MessageKind::ACK, "OK"); }
      catch (const RegistryError& e) { send_frame(conn, MessageKind::ERROR, e.what()); }
    } else if (op == "REG_EP") {
      EndpointDescriptor ep; ep.endpoint_id = EndpointId(std::stoull(p[1])); ep.generation = EndpointGeneration(std::stoull(p[2]));
      ep.protocol = ProtocolId(1); ep.locator.text = "127.0.0.1:" + p[3]; ep.transport = TransportKind::TCP; ep.port = static_cast<std::uint16_t>(std::stoi(p[3]));
      ep.health = Health::HEALTHY; ep.reachability = Reachability::REACHABLE; ep.freshness = Freshness::CURRENT;
      ep.provenance = make_provenance(EvidenceKind::MEASURED, "probe", 0);
      AuthorityEnvelope env; env.epoch = epoch; env.boot = boot;
      try { reg.register_endpoint(ep, env); send_frame(conn, MessageKind::ACK, "OK"); }
      catch (const RegistryError& e) { send_frame(conn, MessageKind::ERROR, e.what()); }
    } else if (op == "REG_CA") {
      CapabilityDescriptor c; c.capability_id = CapabilityId(std::stoull(p[1])); c.kind = capability_kind_from_string(p[2]);
      c.generation = CapabilityGeneration(1); c.version = SemanticVersion::parse("1.0.0").value();
      c.value = CapabilityValue::make_bool(true); c.freshness = Freshness::CURRENT;
      c.provenance = make_provenance(EvidenceKind::MEASURED, "dev", 0);
      AuthorityEnvelope env; env.epoch = epoch; env.boot = boot;
      try { reg.register_capability(c, env); send_frame(conn, MessageKind::ACK, "OK"); }
      catch (const RegistryError& e) { send_frame(conn, MessageKind::ERROR, e.what()); }
    } else if (op == "REG_IN") {
      ServiceInstance inst; inst.service_id = ServiceId(std::stoull(p[1])); inst.service_generation = ServiceGeneration(1);
      inst.instance_generation = ServiceInstanceGeneration(std::stoull(p[2])); inst.node = NodeId(std::stoull(p[3]));
      inst.worker = worker; inst.boot = boot; inst.runtime_id = RuntimeId(std::stoull(p[4])); inst.runtime_instance = RuntimeInstanceId(std::stoull(p[5]));
      inst.endpoints = {EndpointId(std::stoull(p[6]))}; inst.capabilities = {CapabilityId(std::stoull(p[7]))};
      inst.health = Health::HEALTHY; inst.readiness = Readiness::READY; inst.freshness = Freshness::CURRENT;
      inst.reachability = Reachability::REACHABLE; inst.version = SemanticVersion::parse("1.0.0").value();
      inst.provenance = make_provenance(EvidenceKind::REPORTED, "worker", 0); inst.lifecycle = Lifecycle::REGISTERING;
      AuthorityEnvelope env; env.epoch = epoch; env.boot = boot;
      try { ServiceInstanceId id = reg.register_instance(inst, env); send_frame(conn, MessageKind::ACK, "ID=" + std::to_string(id.value())); }
      catch (const RegistryError& e) { send_frame(conn, MessageKind::ERROR, e.what()); }
    } else if (op == "UH") {
      AuthorityEnvelope env; env.epoch = epoch; env.boot = boot; env.service_instance_generation = ServiceInstanceGeneration(std::stoull(p[3]));
      try { reg.update_health(ServiceInstanceId(std::stoull(p[1])), p[2] == "HEALTHY" ? Health::HEALTHY : Health::DEGRADED, env); send_frame(conn, MessageKind::ACK, "OK"); }
      catch (const RegistryError& e) { send_frame(conn, MessageKind::ERROR, e.what()); }
    } else if (op == "UR") {
      AuthorityEnvelope env; env.epoch = epoch; env.boot = boot; env.service_instance_generation = ServiceInstanceGeneration(std::stoull(p[3]));
      try { reg.update_readiness(ServiceInstanceId(std::stoull(p[1])), p[2] == "READY" ? Readiness::READY : Readiness::NOT_READY, env); send_frame(conn, MessageKind::ACK, "OK"); }
      catch (const RegistryError& e) { send_frame(conn, MessageKind::ERROR, e.what()); }
    } else {
      send_frame(conn, MessageKind::ERROR, "UNKNOWN_OP");
    }
  }
  if (has_worker) reg.mark_worker_died(boot);
}

}  // namespace

int main(int argc, char** argv) {
  TcpSocket::startup();
  std::uint16_t port = argc > 1 ? std::uint16_t(std::stoi(argv[1])) : 41817;
  TcpServer server;
  if (!server.listen(port)) { std::printf("COORDINATOR: cannot listen on port %u\n", port); TcpSocket::cleanup(); return 2; }
  Registry reg;
  reg.begin_coordinator_epoch();
  CoordinatorEpoch epoch = reg.authority().epoch;
  std::atomic<bool> stop{false};
  std::printf("COORDINATOR: listening on 127.0.0.1:%u epoch=%llu\n", port, (unsigned long long)epoch.value());
  std::fflush(stdout);
  std::vector<std::thread> threads;
  while (!stop) {
    TcpSocket conn = server.accept();
    if (!conn.valid()) break;
    threads.emplace_back([conn = std::move(conn), &reg, epoch, &stop]() mutable {
      handle_connection(std::move(conn), reg, epoch, stop);
    });
  }
  for (auto& t : threads) t.join();
  TcpSocket::cleanup();
  return 0;
}
