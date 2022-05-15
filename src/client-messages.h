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

#endif  // _CLIENT_MESSAGES_H_
