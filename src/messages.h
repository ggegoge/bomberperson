// Messages sent in our protocol.
// TODO: moving implementation to .cc?

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <variant>
#include <vector>
#include <map>
#include <tuple>
#include <string>

#include "serialise.h"

namespace client_messages {

enum ClientMessageType {
  Join, PlaceBomb, PlaceBlock, Move
};

enum Direction {
  Up, Right, Down, Left,
};

struct Join {
  std::string name;
  Join(const std::string& str) : name(str) {}
};

struct Move {
  Direction direction;
  Move(Direction dir) : direction(dir) {}
};

}; // namespace client_message

// todo: is there point in serialising enums with one field?
namespace server_messages {

using PlayerId = uint8_t;
using BombId = uint32_t;
using Position = std::pair<uint16_t, uint16_t>;
using Score = uint32_t;

template <typename T>
using List = std::vector<T>;

struct Player {
  std::string name;
  std::string address;
};

struct Bomb {
  Position position;
  uint16_t timer;
};

enum EventType {
  BombPlaced, BombExploded, PlayerMoved, BlockPlaced
};

// structures for those events:
struct BombPlaced {
  BombId id;
  Position position;
};

struct BombExploded {
  BombId id;
  List<PlayerId> robots_destroyed;
  List<Position> blocks_destroyed;
};

// redundant perhaps?
struct BlockPlaced {
  Position position;
};

struct PlayerMoved {
  PlayerId id;
  Position position;
};

// TODO: consider having possibility of reading a variant 
using EventVar = std::variant<struct BombPlaced, struct BombExploded,
                             struct PlayerMoved, struct BlockPlaced>;

enum ServerMessageType {
  Hello, AcceptedPlayer, GameStarted, Turn, GameEnded
};

// Hello
struct Hello {
  std::string server_name;
  uint8_t players_count;
  uint16_t size_x;
  uint16_t size_y;
  uint16_t game_length;
  uint16_t explosion_radius;
  uint16_t bomb_timer;
};


struct AcceptedPlayer {
  PlayerId id;
  Player player;
};

// also redundant?
struct GameStarted {
  // todo: using map?
  std::map<PlayerId, Player> players;
};

struct Turn {
  uint16_t turn;
  List<EventVar> events;
};

struct GameEnded {
  std::map<PlayerId, Score> scores;
};

}; // namespace server_messagess

// ok now, adding overloads for server_messagess where needed
// this is going to be painful innit...

Ser& operator<<(Ser& ser, const struct client_messages::Join& j);

Ser& operator<<(Ser& ser, const struct client_messages::Move& m);

Ser& operator<<(Ser& ser, const struct server_messages::Hello& hello);

Ser& operator<<(Ser& ser, const struct server_messages::AcceptedPlayer& ap);

Ser& operator<<(Ser& ser, const struct server_messages::GameStarted& gs);

Ser& operator<<(Ser& ser, const struct server_messages::Turn& turn);

Ser& operator<<(Ser& ser, const struct server_messages::GameEnded& ge);

Ser& operator<<(Ser& ser, const server_messages::Position& position);

Ser& operator<<(Ser& ser, const struct server_messages::BombPlaced& bp);

Ser& operator<<(Ser& ser, const struct server_messages::BombExploded& be);

Ser& operator<<(Ser& ser, const struct server_messages::PlayerMoved& pm);

Ser& operator<<(Ser& ser, const struct server_messages::BlockPlaced& bp);

Ser& operator<<(Ser& ser, const server_messages::EventVar& ev);

// All event types may be serialised!

namespace gui_message {

}; // namespace gui_message

#endif  // _MESSAGES_H_
