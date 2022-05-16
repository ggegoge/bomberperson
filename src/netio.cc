#include "sockets.h"
#include "netio.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <cstddef>
#include <cstdint>
#include <unistd.h>
#include <vector>

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

void ReaderUDP::recv_from_sock(SocketUDP& sock)
{
  buff_size = sock.receive_message(buff, UDP_DATAGRAM_SIZE);
}

// boost
std::vector<uint8_t> ReaderUDPboost::read(size_t nbytes)
{
  if (pos + nbytes > buff_size)
    throw std::runtime_error("Not enough bytes in the buffer!");

  std::vector<uint8_t> bytes(buff + pos, buff + pos + nbytes);
  pos += nbytes;
  return bytes;
}

bool ReaderUDPboost::eof() const
{
  return pos == buff_size;
}

using boost::asio::ip::udp;

// assuming the socket has been connected to proper endpoint
void ReaderUDPboost::recv_from_sock(udp::socket& sock)
{
  buff_size = sock.receive(boost::asio::buffer(buff));
}


bool ReaderTCPboost::eof() const
{
  return false;
}

std::vector<uint8_t> ReaderTCPboost::read(size_t nbytes)
{
  std::vector<uint8_t> bytes;
  boost::asio::read(sock, boost::asio::buffer(bytes, nbytes));
  return bytes;
}
