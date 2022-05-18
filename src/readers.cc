// Implementation of methods for reading bytes from sockets.

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "readers.h"

using boost::asio::ip::udp;

std::vector<uint8_t> ReaderUDP::read(size_t nbytes)
{
  if (pos + nbytes > buff_size)
    throw std::runtime_error("Not enough bytes in the buffer!");

  std::vector<uint8_t> bytes(buff + pos, buff + pos + nbytes);
  pos += nbytes;
  return bytes;
}

void ReaderUDP::sock_fill(udp::socket& sock, udp::endpoint& endp)
{
  buff_size = sock.receive_from(boost::asio::buffer(buff), endp);
  pos = 0;
}

std::vector<uint8_t> ReaderTCP::read(size_t nbytes)
{
  std::vector<uint8_t> bytes(nbytes);
  boost::asio::read(sock, boost::asio::buffer(bytes, nbytes));
  return bytes;
}
