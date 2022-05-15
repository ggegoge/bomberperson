#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// There are problems on macos with changing the byte order.
#ifdef __MACH__
#  include <libkern/OSByteOrder.h>
#  define htobe64(x) OSSwapHostToBigInt64(x)
#  define be64toh(x) OSSwapBigInt64ToHost(x)
#else
#  include <endian.h>
#endif // __MACH__

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "sockets.h"

SocketUDP::SocketUDP(uint16_t port)
{
  fd = socket(AF_INET, SOCK_DGRAM, 0);

  if (fd < 0)
    throw std::runtime_error("failed to create a socket.");

  sockaddr_in server_address;

  // IPv4
  server_address.sin_family = AF_INET;
  // listening on all interfaces
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(port);

  // Bind the socket to an address.
  int err = bind(fd, (sockaddr*)&server_address,
                 (socklen_t)sizeof(server_address));

  if (err)
    throw std::runtime_error("failed to bind the socket");
}

// General purpose message receiving function.
size_t SocketUDP::receive_message(void* buffer, size_t max_length) const
{
  socklen_t address_length = (socklen_t)sizeof(client_addr);
  ssize_t len = recvfrom(fd, buffer, max_length, no_flags,
                         (sockaddr*)&client_addr, &address_length);

  if (len < 0)
    throw std::runtime_error("error in receiving a message");

  return (size_t)len;
}

// General purpose sending function.
void SocketUDP::send_message(const void* message, size_t length) const
{
  socklen_t address_length = (socklen_t)sizeof(client_addr);
  ssize_t sent_length = sendto(fd, message, length, no_flags,
                               (sockaddr*)&client_addr, address_length);

  if (sent_length == -1)
    throw std::runtime_error("error in sending a message");

  if (sent_length != (ssize_t)length)
    throw std::runtime_error("the whole message could not be sent");
}

void SocketUDP::send_message(const std::vector<uint8_t>& bytes) const
{
  send_message(bytes.data(), bytes.size());
}

char* SocketUDP::client_ip() const
{
  return inet_ntoa(client_addr.sin_addr);
};

void SocketUDP::close()
{
  ::close(fd);
}

SocketUDP::~SocketUDP() noexcept
{
  ::close(fd);
}
