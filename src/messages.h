// Messages sent in our protocol.

// TODO: reorganise the namespaces so that they are less messy?
// one namespace robots? with sub name spaces perhaps?
// or something else?
// Also: enums -> capitals and drop the "struct"?

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <variant>
#include <map>
#include <set>
#include <unordered_set>
#include <tuple>
#include <string>
#include <vector>

#include "serialise.h"

using PlayerId = uint8_t;
using BombId = uint32_t;
using Position = std::pair<uint16_t, uint16_t>;
using Score = uint32_t;

class DeserProtocolError : public std::runtime_error {
public:
  DeserProtocolError()
    : std::runtime_error("Error in deserialisaion according to robots protocol") {}
  DeserProtocolError(const std::string& msg) : std::runtime_error(msg) {}
};

namespace client_messages
{

enum ClientMessageType {
  JOIN, PLACE_BOMB, PLACE_BLOCK, MOVE
};

enum Direction {
  UP, RIGHT, DOWN, LEFT,
};

struct Join {
  std::string name;
  Join(const std::string& str) : name(str) {}
  Join() : name() {}
};

// Placeholders.
struct PlaceBomb {};
struct PlaceBlock {};

struct Move {
  Direction direction;
  Move(Direction dir) : direction(dir) {}
  Move() : direction() {}
};

using ClientMessage =
  std::variant<struct Join, struct PlaceBomb, struct PlaceBlock, struct Move>;

}; // namespace client_message

namespace server_messages
{

struct Player {
  std::string name;
  std::string address;
};

struct Bomb {
  Position position;
  uint16_t timer;
  bool operator==(const struct Bomb& other) const
  {
    return position == other.position;
  }

  auto operator<=>(const struct Bomb& other) const
  {
    return position <=> other.position;
  }
};

enum EventType {
  BOMB_PLACED, BOMB_EXPLODED, PLAYER_MOVED, BLOCK_PLACED
};

// structures for those events:
struct BombPlaced {
  BombId id;
  Position position;

  bool operator==(const struct BombPlaced& other) const
  {
    return id == other.id;
  }

  auto operator<=>(const struct BombPlaced& other) const
  {
    return id <=> other.id;
  }
};

struct BombExploded {
  BombId id;
  std::set<PlayerId> killed;
  std::set<Position> blocks_destroyed;
};

// redundant perhaps?
struct BlockPlaced {
  Position position;
};

struct PlayerMoved {
  PlayerId id;
  Position position;
};

using EventVar =
  std::variant<struct BombPlaced, struct BombExploded, struct PlayerMoved,
        struct BlockPlaced>;

enum ServerMessageType {
  HELLO, ACCEPTED_PLAYER, GAME_STARTED, TURN, GAME_ENDED
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
  std::map<PlayerId, Player> players;
};

struct Turn {
  uint16_t turn;
  std::vector<EventVar> events;
};

struct GameEnded {
  std::map<PlayerId, Score> scores;
};

using ServerMessage = std::variant<struct Hello, struct AcceptedPlayer,
          struct GameStarted, struct Turn, struct GameEnded>;

}; // namespace server_messagess

namespace display_messages
{

enum DisplayMessageType {
  LOBBY, GAME
};

struct Lobby {
  std::string server_name;
  uint8_t players_count;
  uint16_t size_x;
  uint16_t size_y;
  uint16_t game_length;
  uint16_t explosion_radius;
  uint16_t bomb_timer;
  std::map<PlayerId, server_messages::Player> players;
};

struct Game {
  std::string server_name;
  uint16_t size_x;
  uint16_t size_y;
  uint16_t game_length;
  uint16_t turn;
  std::map<PlayerId, server_messages::Player> players;
  std::map<PlayerId, Position> player_positions;
  std::set<Position> blocks;
  // todo: better namespacing would be useful, namespace robots for the commons
  std::set<server_messages::Bomb> bombs;
  std::set<Position> explosions;
  std::map<PlayerId, Score> scores;
};

using DisplayMessage = std::variant<struct Lobby, struct Game>;

}; // namespace display_message

namespace input_messages
{

enum InputMessageType {
  PLACE_BOMB, PLACE_BLOCK, MOVE
};

using InputMessage =
  std::variant<struct client_messages::PlaceBomb, struct client_messages::PlaceBlock,
      struct client_messages::Move>;

}; // namespace input_messages

// For display messages I need only serialisation as I only send those messages.
Serialiser& operator<<(Serialiser& ser, const display_messages::DisplayMessage& msg);

// For input messages I actually only need deserialisation as I only receive those
// but for now serialisation is also implemented for testing purposes.
Serialiser& operator<<(Serialiser& ser, const input_messages::InputMessage& msg);

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, input_messages::InputMessage& msg);

// Serialisation and deserialisation of client messages.
Serialiser& operator<<(Serialiser& ser, const client_messages::ClientMessage& msg);

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, client_messages::ClientMessage& msg);

// Serialisation and deserialisation of server messages.
Serialiser& operator<<(Serialiser& ser, const server_messages::ServerMessage& msg);

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, server_messages::ServerMessage& msg);

#endif  // _MESSAGES_H_
