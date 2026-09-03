#include <runtimeregistry/protocol.hpp>
#include <runtimeregistry/tcp.hpp>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

using namespace runtimeregistry;

namespace {
bool send_text(TcpSocket& s, MessageKind k, const std::string& t) {
  Frame f; f.kind = k; f.payload.assign(t.begin(), t.end());
  std::vector<std::uint8_t> enc = encode_frame(f);
  return s.send(enc.data(), enc.size());
}
std::string recv_line(TcpSocket& s) {
  std::uint8_t hdr[10];
  int n = s.recv(hdr, 10);
  if (n != 10) return {};
  std::uint32_t len = static_cast<std::uint32_t>(hdr[6]) | (static_cast<std::uint32_t>(hdr[7]) << 8) |
                      (static_cast<std::uint32_t>(hdr[8]) << 16) | (static_cast<std::uint32_t>(hdr[9]) << 24);
  if (len > kMaxPayload) return {};
  std::vector<std::uint8_t> body(10 + len + 4);
  std::memcpy(body.data(), hdr, 10);
  std::size_t got = 10;
  while (got < body.size()) { n = s.recv(body.data() + got, body.size() - got); if (n <= 0) return {}; got += n; }
  auto dec = decode_frame(body.data(), body.size());
  if (!dec) return {};
  return std::string(reinterpret_cast<const char*>(dec->payload.data()), dec->payload.size());
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) { std::printf("worker: <port> <workerId> <mode>\n"); return 2; }
  std::uint16_t port = std::uint16_t(std::stoi(argv[1]));
  int workerId = std::stoi(argv[2]);
  std::string mode = argv[3];
  std::string gen = (argc > 4 && std::string(argv[4]).size() > 0) ? argv[4] : "1";

  TcpSocket::startup();
  TcpSocket conn;
  if (!tcp_connect(conn, port)) { std::printf("WORKER%d: connect failed\n", workerId); TcpSocket::cleanup(); return 3; }

  send_text(conn, MessageKind::HELLO, "HELLO," + std::to_string(workerId));
  std::string boot_line = recv_line(conn);
  std::size_t bootv = 0;
  if (boot_line.rfind("BOOT=", 0) == 0) bootv = std::stoull(boot_line.substr(5));
  std::printf("WORKER%d: boot=%llu\n", workerId, (unsigned long long)bootv);
  std::fflush(stdout);

  if (mode == "hold") {
    send_text(conn, MessageKind::REGISTER_RUNTIME, "REG_RT," + std::to_string(workerId) + "," + std::to_string(workerId) +
              ",1,native,cpp,x64"); recv_line(conn);
    send_text(conn, MessageKind::REGISTER_SERVICE, "REG_SV," + std::to_string(100 + workerId) + ",1,MODEL_SERVER,svc,CUDA"); recv_line(conn);
    send_text(conn, MessageKind::REGISTER_ENDPOINT, "REG_EP," + std::to_string(workerId) + "," + gen + "," + std::to_string(41000 + workerId)); recv_line(conn);
    send_text(conn, MessageKind::REGISTER_CAPABILITY, "REG_CA," + std::to_string(workerId) + ",CUDA_AVAILABLE"); recv_line(conn);
    send_text(conn, MessageKind::REGISTER_SERVICE, "REG_IN," + std::to_string(100 + workerId) + "," + gen + ",1," +
              std::to_string(workerId) + "," + std::to_string(workerId) + "," + std::to_string(workerId) + "," + std::to_string(workerId));
    std::string ack = recv_line(conn);
    std::printf("WORKER%d: registered and holding (ack=%s)\n", workerId, ack.c_str());
    std::fflush(stdout);
    while (true) { std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
  } else if (mode == "idle") {
    while (true) { std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
  }
  return 0;
}
