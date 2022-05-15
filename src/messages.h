// Messages sent in our protocol.

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

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

#endif  // _MESSAGES_H_
