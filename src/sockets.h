// This file will contain the definitions of classes used to represent sockets
// (both UDP and TCP). Have this here in a separate file so that I can switch
// between boost sockets and old C sockets wheras other modules can just refer
// to those two.

#ifndef _SOCKETS_H_
#define _SOCKETS_H_

#include <arpa/inet.h>
#include <cstdint>
#include <vector>

// Declaration of a basic UDP socket based on the C-interface.
// TODO: boost?
class SocketUDP {
  int fd;
  sockaddr_in client_addr;
  static constexpr int no_flags = 0;

public:
  SocketUDP(uint16_t port);
  // Construct a socket with a chosen adress to listen on and send to.
  SocketUDP(uint16_t port, const char* host_ip, uint16_t host_port);  
  ~SocketUDP() noexcept;
  size_t receive_message(void* buffer, size_t max_length) const;
  void send_message(const void* message, size_t length) const;
  void send_message(const std::vector<uint8_t>& bytes) const;
  char* client_ip() const;
  void close();
};

// TODO
class SocketTCP {
public:
  void send_message(const std::vector<uint8_t>& bytes) const;
  std::vector<uint8_t> read_bytes(size_t nbytes) const;
};

#endif  // _SOCKETS_H_
