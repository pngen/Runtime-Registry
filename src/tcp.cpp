#include <runtimeregistry/tcp.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

namespace runtimeregistry {

void TcpSocket::startup() {
  WSADATA d;
  WSAStartup(MAKEWORD(2, 2), &d);
}
void TcpSocket::cleanup() { WSACleanup(); }

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : handle_(other.handle_) { other.handle_ = kInvalid; }
TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
  if (this != &other) { reset(); handle_ = other.handle_; other.handle_ = kInvalid; }
  return *this;
}
TcpSocket::~TcpSocket() { reset(); }
void TcpSocket::reset() noexcept {
  if (handle_ != kInvalid) { closesocket(handle_); handle_ = kInvalid; }
}
TcpSocket TcpSocket::from_raw(int handle) noexcept {
  TcpSocket s; s.handle_ = handle; return s;
}

int TcpSocket::recv(void* buf, std::size_t len) {
  if (handle_ == kInvalid) return -1;
  return ::recv(handle_, reinterpret_cast<char*>(buf), static_cast<int>(len), 0);
}
bool TcpSocket::send(const void* buf, std::size_t len) {
  if (handle_ == kInvalid) return false;
  const char* p = reinterpret_cast<const char*>(buf);
  std::size_t total = 0;
  while (total < len) {
    int n = ::send(handle_, p + total, static_cast<int>(len - total), 0);
    if (n <= 0) return false;
    total += static_cast<std::size_t>(n);
  }
  return true;
}

TcpServer::~TcpServer() { listener_.reset(); }

bool TcpServer::listen(std::uint16_t port) {
  listener_.reset();
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) return false;
  listener_ = TcpSocket::from_raw(static_cast<int>(s));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::bind(listener_.raw(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { listener_.reset(); return false; }
  if (::listen(listener_.raw(), SOMAXCONN) != 0) { listener_.reset(); return false; }
  port_ = port;
  return true;
}

TcpSocket TcpServer::accept() {
  if (listener_.raw() == -1) return TcpSocket{};
  SOCKET s = ::accept(listener_.raw(), nullptr, nullptr);
  if (s == INVALID_SOCKET) return TcpSocket{};
  return TcpSocket::from_raw(static_cast<int>(s));
}

bool tcp_connect(TcpSocket& out, std::uint16_t port, int timeout_ms) {
  (void)timeout_ms;
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int r = ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  if (r != 0) { closesocket(s); return false; }
  out = TcpSocket::from_raw(static_cast<int>(s));
  return true;
}

}  // namespace runtimeregistry
