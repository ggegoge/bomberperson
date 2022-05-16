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

// 
constexpr size_t UDP_DATAGRAM_SIZE = 65507;

// This just serves as a readable byte buffer that can be filled with a chosen
// UDP socket.
class ReaderUDP_c {
  size_t pos = 0;
  size_t buff_size = 0;
  uint8_t buff[UDP_DATAGRAM_SIZE];
public:
  ReaderUDP_c() {}

  bool eof() const;
  
  void recv_from_sock(SocketUDP& sock);

  std::vector<uint8_t> read(size_t nbytes);
};

// This is just an interface that allows us to read through a TCP socket.
// Not sure yet how exactly will I do this. Perhaps I will merge this with
// the SocketTCP as it may be much easier then have this with a reference to
// the socket? As writing will be done through the socket innit.
// TODO
class ReaderTCP_c {
  SocketTCP& sock;
public:
  ReaderTCP_c(SocketTCP& sock) : sock(sock) {}
  std::vector<uint8_t> read(size_t nbytes);
};

#endif  // _SOCKETS_H_
