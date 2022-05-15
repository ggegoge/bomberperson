// Messages sent by the client and functions to serialise or deserialise them.

#ifndef _CLIENT_MESSAGES_H_
#define _CLIENT_MESSAGES_H_

#include "serialise.h"

namespace client_messages {

enum ClientMessage {
  Join, PlaceBomb, PlaceBlock, Move
};

enum Direction {
  Up, Right, Down, Left,
};


struct DrawMessage {
  
};

};

// // Overloading enum serialisation as uint8_t is not the default for enums...
// Ser& operator<<(Ser& ser, client_messages::ClientMessage cm)
// {
//   return ser << (uint8_t)cm;
// }

// Ser& operator<<(Ser& ser, client_messages::Direction dir)
// {
//   return ser << (uint8_t)dir;
// }


#endif  // _CLIENT_MESSAGES_H_
