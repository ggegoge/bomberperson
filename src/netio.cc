#include "sockets.h"
#include "netio.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <vector>

std::vector<uint8_t> ReaderUDP_c::read(size_t nbytes)
{
  if (pos + nbytes > buff_size)
    throw std::runtime_error("Not enough bytes in the buffer!");

  std::vector<uint8_t> bytes(buff + pos, buff + pos + nbytes);
  pos += nbytes;
  return bytes;
}

bool ReaderUDP_c::eof() const
{
  return pos == buff_size;
}

void ReaderUDP_c::recv_from_sock(SocketUDP& sock)
{
  buff_size = sock.receive_message(buff, UDP_DATAGRAM_SIZE);
}

// boost
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

using boost::asio::ip::udp;

// assuming the socket has been connected to proper endpoint
void ReaderUDP::recv_from_sock(udp::socket& sock)
{
  buff_size = sock.receive(boost::asio::buffer(buff));
}

bool ReaderTCP::eof() const
{
  return false;
}

std::vector<uint8_t> ReaderTCP::read(size_t nbytes)
{
  std::cerr << "going to read " << nbytes << "\n";
  std::vector<uint8_t> bytes(nbytes);
  boost::asio::read(sock, boost::asio::buffer(bytes, nbytes));
  // std::cerr << "size of bytes is " << bytes.size() << "\n";
  return bytes;
}
