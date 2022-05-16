// Messages sent in our protocol.
// TODO: moving implementation to .cc?

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <variant>
#include <vector>
#include <map>
#include <tuple>
#include <string>

#include "serialise.h"

template <typename T>
using List = std::vector<T>;
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
  Join, PlaceBomb, PlaceBlock, Move
};

enum Direction {
  Up, Right, Down, Left,
};

struct Join {
  std::string name;
  Join(const std::string& str) : name(str) {}
};

// Placeholders.
struct PlaceBomb {};
struct PlaceBlock {};

struct Move {
  Direction direction;
  Move(Direction dir) : direction(dir) {}
};

// note: the middle two structs do not exist...
using ClientMessage =
  std::variant<struct Join, struct PlaceBomb, struct PlaceBlock, struct Move>;

}; // namespace client_message

// todo: is there point in serialising enums with one field?
namespace server_messages
{

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
  List<PlayerId> killed;
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

using EventVar =
  std::variant<struct BombPlaced, struct BombExploded, struct PlayerMoved,
        struct BlockPlaced>;

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

using ServerMessage = std::variant<struct Hello, struct AcceptedPlayer,
          struct GameStarted, struct Turn, struct GameEnded>;

}; // namespace server_messagess

namespace display_messages
{

enum DisplayMessageType {
  Lobby, Game
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
  List<Position> blocks;
  // todo: better namespacing would be useful, namespace robots for the commons
  List<server_messages::Bomb> bombs;
  List<Position> explosions;
  std::map<PlayerId, Score> scores;
};

using DisplayMessage = std::variant<struct Lobby, struct Game>;

}; // namespace display_message

namespace input_messages
{

enum InputMessageType {
  PlaceBomb, PlaceBlock, Move
};

using InputMessage =
  std::variant<struct client_messages::PlaceBomb, struct client_messages::PlaceBlock,
      struct client_messages::Move>;

}; // namespace input_messages

// ok now, adding overloads for server_messagess where needed
// this is going to be painful innit...

// Main functions I'd advise (to meself) that I use. Do I need them though?
// perhaps they'r enot really necessary here...
// although having the variant might be nice --> handling with visits?
Serialiser& operator<<(Serialiser& ser, const client_messages::ClientMessage& msg);
Serialiser& operator<<(Serialiser& ser, const server_messages::ServerMessage& msg);
Serialiser& operator<<(Serialiser& ser, const display_messages::DisplayMessage& msg);
Serialiser& operator<<(Serialiser& ser, const input_messages::InputMessage& msg);

// Deserialisation is analogous...
template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, server_messages::ServerMessage& msg);

#endif  // _MESSAGES_H_
