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

void ReaderUDP::recv_from_sock(SocketUDP &sock)
{
  pos = sock.receive_message(buff, UDP_DATAGRAM_SIZE);
}
