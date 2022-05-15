
#include "sockets.h"
#include "netio.h"
#include "serialise.h"
#include "client-messages.h"

#include <cstdio>
#include <iostream>

using namespace std;

ostream& operator<<(ostream& os, vector<uint8_t>& bytes)
{
  os << "[ ";

  for (uint8_t b : bytes)
    os << (int)b << " ";

  os << "]";
  return os;
}

int main(int argc, char* argv[])
{
  if (argc < 3) {
    printf("Usage: %s <port> <host> <host-port> ...\n", argv[0]);
    exit(1);
  }

  uint16_t port = std::stol(argv[1]);
  char* host = argv[2];
  uint16_t host_port = std::stol(argv[3]);
  SocketUDP sock(port, host, host_port);

  Ser ser;
  std::vector<uint8_t> bytes ;

  // wyślemy Join("siemkaaa") -> [0, 8, s, i, ..., a]
  std::string mess{"siemkaaa"};
  ser << client_messages::Join << mess;
  // alternatywnie:
  // ser.ser((uint8_t)client_messages::Join);
  // ser.ser(mess);
  bytes = ser.to_bytes();
  cout << bytes << "\n";
  sock.send_message(bytes);
  ser.clean();

  // Teraz PlaceBomb
  ser << client_messages::PlaceBomb;
  bytes = ser.to_bytes();
  cout << bytes << "\n";
  sock.send_message(bytes);
  ser.clean();

  // Teraz PlaceBlock
  ser << client_messages::PlaceBlock;
  bytes = ser.to_bytes();
  cout << bytes << "\n";
  sock.send_message(bytes);
  ser.clean();

  // Teraz Move(Left)
  ser << client_messages::Move << client_messages::Left;
  bytes = ser.to_bytes();
  cout << bytes << "\n";
  sock.send_message(bytes);
  ser.clean();  
  
  return 0;
}
