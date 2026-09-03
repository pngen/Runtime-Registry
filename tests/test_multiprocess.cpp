#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <runtimeregistry/protocol.hpp>
#include <runtimeregistry/tcp.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <windows.h>

using namespace runtimeregistry;

namespace {
int g_fail = 0;
void check(bool ok, const char* what) { std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what); std::fflush(stdout); if (!ok) ++g_fail; }

HANDLE spawn(const std::string& exe, const std::string& args) {
  std::string cmd = "\"" + exe + "\" " + args;
  STARTUPINFOA si{}; si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::vector<char> buf(cmd.begin(), cmd.end()); buf.push_back('\0');
  if (!CreateProcessA(exe.c_str(), buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    return nullptr;
  return pi.hProcess;
}

bool send_text(TcpSocket& s, const std::string& t) {
  Frame f; f.kind = MessageKind::QUERY; f.payload.assign(t.begin(), t.end());
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

int query_count(std::uint16_t port) {
  TcpSocket c;
  if (!tcp_connect(c, port)) return -1;
  send_text(c, "QUERY,MODEL_SERVER");
  std::string r = recv_line(c);
  if (r.find("RESULT_") != 0) return -1;
  int count = 0;
  std::size_t pos = r.find(',');
  if (pos == std::string::npos) return 0;
  while (pos != std::string::npos) { ++count; std::size_t nxt = r.find(',', pos + 1); if (nxt == std::string::npos) break; pos = nxt; }
  return count;
}

bool wait_target(std::uint16_t port, int target, int timeout_ms) {
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
    int c = query_count(port);
    if (c == target) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
  }
  return false;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) { std::printf("test_multiprocess: <coordinator> <worker>\n"); return 2; }
  std::string coord = argv[1];
  std::string worker = argv[2];
  std::uint16_t port = 42117;

  std::printf("[multiprocess]\n");
  TcpSocket::startup();

  HANDLE hcoord = spawn(coord, std::to_string(port));
  check(hcoord != nullptr, "coordinator process started");
  { auto start = std::chrono::steady_clock::now(); bool ready = false;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(6000)) {
      TcpSocket c; if (tcp_connect(c, port)) { ready = true; break; }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    check(ready, "coordinator accepted loopback TCP"); }

  HANDLE hA = spawn(worker, std::to_string(port) + " 1 hold 1");
  HANDLE hB = spawn(worker, std::to_string(port) + " 2 hold 1");
  check(hA != nullptr, "worker A process started");
  check(hB != nullptr, "worker B process started");

  check(wait_target(port, 2, 8000), "query discovers both workers after registration");

  TerminateProcess(hA, 0);
  WaitForSingleObject(hA, 5000);
  CloseHandle(hA);
  std::printf("  worker A terminated as real OS process\n");

  check(wait_target(port, 1, 8000), "query falls back to Worker B after A dies");

  HANDLE hA2 = spawn(worker, std::to_string(port) + " 1 hold 2");
  check(hA2 != nullptr, "worker A restarted with fresh incarnation");
  check(wait_target(port, 2, 8000), "fresh Worker A rejoined and discoverable alongside B");

  if (hB) { TerminateProcess(hB, 0); WaitForSingleObject(hB, 10000); CloseHandle(hB); }
  if (hcoord) { TerminateProcess(hcoord, 0); WaitForSingleObject(hcoord, 10000); CloseHandle(hcoord); }
  if (hA2) { TerminateProcess(hA2, 0); WaitForSingleObject(hA2, 10000); CloseHandle(hA2); }

  TcpSocket::cleanup();
  std::printf("multiprocess: %s\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
  return g_fail == 0 ? 0 : 1;
}
