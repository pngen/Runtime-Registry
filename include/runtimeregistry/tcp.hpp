#pragma once
// Minimal Windows loopback TCP client/server used by the multiprocess authority
// proof. Serializes writes per connection; blocking I/O is never done while
// holding a global registry lock.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace runtimeregistry {

class TcpSocket {
 public:
  TcpSocket() = default;
  ~TcpSocket();
  TcpSocket(const TcpSocket&) = delete;
  TcpSocket& operator=(const TcpSocket&) = delete;
  TcpSocket(TcpSocket&& other) noexcept;
  TcpSocket& operator=(TcpSocket&& other) noexcept;

  [[nodiscard]] bool valid() const noexcept { return handle_ != kInvalid; }
  [[nodiscard]] int raw() const noexcept { return handle_; }
  void reset() noexcept;

  // Adopts a raw OS socket handle (e.g. from socket()/accept()).
  [[nodiscard]] static TcpSocket from_raw(int handle) noexcept;

  // Returns number of bytes read, 0 on orderly close, -1 on error/closed.
  [[nodiscard]] int recv(void* buf, std::size_t len);
  [[nodiscard]] bool send(const void* buf, std::size_t len);

  static void startup();
  static void cleanup();

 private:
  static constexpr int kInvalid = -1;
  int handle_ = kInvalid;
};

// A listening loopback server on 127.0.0.1:port.
class TcpServer {
 public:
  ~TcpServer();  // closes listener
  [[nodiscard]] bool listen(std::uint16_t port);
  [[nodiscard]] TcpSocket accept();
  [[nodiscard]] std::uint16_t port() const { return port_; }

 private:
  TcpSocket listener_;
  std::uint16_t port_{0};
};

// Connect a client to 127.0.0.1:port.
[[nodiscard]] bool tcp_connect(TcpSocket& out, std::uint16_t port, int timeout_ms = 5000);

}  // namespace runtimeregistry
