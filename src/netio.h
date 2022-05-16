// The netio module serves as an interface for reading pure bytes from sockets
// and buffers. The classes here are written in such a manner that they can be
// used by the serialisation module.

#ifndef _NETIO_H_
#define _NETIO_H_

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

  bool eof() const;
  
  void recv_from_sock(boost::asio::ip::udp::socket& sock);

  void recv_from_sock(boost::asio::ip::udp::socket& sock,
                      boost::asio::ip::udp::endpoint& endp);

  std::vector<uint8_t> read(size_t nbytes);
};

class ReaderTCP {
  boost::asio::ip::tcp::socket& sock;
public:
  ReaderTCP(boost::asio::ip::tcp::socket& sock) : sock(sock) {}

  std::vector<uint8_t> read(size_t nbytes);
};

#endif  // _NETIO_H_
