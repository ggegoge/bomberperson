// The readers module serves as an interface for reading pure bytes from sockets
// and buffers. The classes here are written in such a manner that they can be
// used by the serialisation module (ie they satisfy the "Readable" concept).

#ifndef _READERS_H_
#define _READERS_H_

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr size_t UDP_DATAGRAM_SIZE = 65507;

class ReaderUDP {
  size_t pos = 0;
  size_t buff_size = 0;
  uint8_t buff[UDP_DATAGRAM_SIZE];
public:
  ReaderUDP() {}

  // Fill the reader with a udp socket.
  void sock_fill(boost::asio::ip::udp::socket& sock);

  std::vector<uint8_t> read(size_t nbytes);
  size_t avalaible() const;
};

class ReaderTCP {
  boost::asio::ip::tcp::socket& sock;
public:
  ReaderTCP(boost::asio::ip::tcp::socket& sock) : sock(sock) {}

  std::vector<uint8_t> read(size_t nbytes);
  size_t avalaible() const;
};

#endif  // _READERS_H_
