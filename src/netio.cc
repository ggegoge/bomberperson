// Implementation of methods for reading bytes from sockets.

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "netio.h"

using boost::asio::ip::udp;

std::vector<uint8_t> ReaderUDP::read(size_t nbytes)
{
  if (pos + nbytes > buff_size)
    throw std::runtime_error("Not enough bytes in the buffer!");

  std::vector<uint8_t> bytes(buff + pos, buff + pos + nbytes);
  pos += nbytes;
  return bytes;
}

bool ReaderUDP::eof() const
{
  return pos == buff_size;
}

// assuming the socket has been connected to proper endpoint
void ReaderUDP::recv_from_sock(udp::socket& sock)
{
  buff_size = sock.receive(boost::asio::buffer(buff));
}

void ReaderUDP::recv_from_sock(udp::socket& sock, udp::endpoint& endp)
{
  buff_size = sock.receive_from(boost::asio::buffer(buff), endp);
}

std::vector<uint8_t> ReaderTCP::read(size_t nbytes)
{
  std::vector<uint8_t> bytes(nbytes);
  boost::asio::read(sock, boost::asio::buffer(bytes, nbytes));
  return bytes;
}
