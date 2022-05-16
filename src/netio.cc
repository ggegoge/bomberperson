#include "sockets.h"
#include "netio.h"

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

void ReaderUDP::recv_from_sock(SocketUDP &sock)
{
  buff_size = sock.receive_message(buff, UDP_DATAGRAM_SIZE);
}
