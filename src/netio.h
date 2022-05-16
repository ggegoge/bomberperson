// The netio module serves as an interface for reading pure bytes from sockets
// and buffers. The classes here are written in such a manner that they can be
// used by the serialisation module. This is just another layer of abstraction.

#ifndef _NETIO_H_
#define _NETIO_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>

#include "sockets.h"

constexpr size_t UDP_DATAGRAM_SIZE = 65507;

// This just serves as a readable byte buffer that can be filled with a chosen
// UDP socket.
class ReaderUDP {
  size_t pos = 0;
  size_t buff_size = 0;
  uint8_t buff[UDP_DATAGRAM_SIZE];
public:
  ReaderUDP() {}

  bool eof() const;
  
  void recv_from_sock(SocketUDP& sock);

  std::vector<uint8_t> read(size_t nbytes);
};

// This is just an interface that allows us to read through a TCP socket.
// Not sure yet how exactly will I do this. Perhaps I will merge this with
// the SocketTCP as it may be much easier then have this with a reference to
// the socket? As writing will be done through the socket innit.
// TODO
class ReaderTCP {
  SocketTCP& sock;
public:
  ReaderTCP(SocketTCP& sock) : sock(sock) {}
  std::vector<uint8_t> read(size_t nbytes);
};

#endif  // _NETIO_H_
