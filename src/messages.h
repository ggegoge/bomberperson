// Messages sent in our protocol and (de)serialisers for them.

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <variant>
#include <map>
#include <set>
#include <unordered_set>
#include <tuple>
#include <string>
#include <vector>

#include "marshal.h"

// todo: std tuple and usings instead of structs and thus easier serialisation?
// but then using std::get everywhere would be a nigthmare, wouldn't it?
// decomposition though?

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

// struct Join {
//   std::string name;
//   Join(const std::string& str) : name(str) {}
//   Join() : name() {}
// };

using Join = std::string;

// Placeholders.
struct PlaceBomb {};
struct PlaceBlock {};

// using PlaceBomb = std::tuple<>;
// using PlaceBlock = std::tuple<>;

// struct Move {
//   Direction direction;
//   Move(Direction dir) : direction(dir) {}
//   Move() : direction() {}
// };

using Move = Direction;

using ClientMessage = std::variant<Join, PlaceBomb, PlaceBlock, Move>;

}; // namespace client_message

namespace server_messages
{

// struct Player {
//   std::string name;
//   std::string address;
// };

using Player = std::pair<std::string, std::string>;

// struct Bomb {
//   Position position;
//   uint16_t timer;
//   bool operator==(const Bomb& other) const
//   {
//     return position == other.position;
//   }

//   auto operator<=>(const Bomb& other) const
//   {
//     return position <=> other.position;
//   }
// };

using Bomb = std::pair<Position, uint16_t>;

enum EventType {
  BOMB_PLACED, BOMB_EXPLODED, PLAYER_MOVED, BLOCK_PLACED
};

// structures for those events:
// struct BombPlaced {
//   BombId id;
//   Position position;

//   bool operator==(const BombPlaced& other) const
//   {
//     return id == other.id;
//   }

//   auto operator<=>(const BombPlaced& other) const
//   {
//     return id <=> other.id;
//   }
// };

using BombPlaced = std::pair<BombId, Position>;

// struct BombExploded {
//   BombId id;
//   std::set<PlayerId> killed;
//   std::set<Position> blocks_destroyed;
// };

using BombExploded = std::tuple<BombId, std::set<PlayerId>, std::set<Position>>;

// redundant perhaps?
// struct BlockPlaced {
//   Position position;
// };

using BlockPlaced = Position;

// struct PlayerMoved {
//   PlayerId id;
//   Position position;
// };

using PlayerMoved = std::pair<PlayerId, Position>;

using Event = std::variant<BombPlaced, BombExploded, PlayerMoved, BlockPlaced>;

enum ServerMessageType {
  HELLO, ACCEPTED_PLAYER, GAME_STARTED, TURN, GAME_ENDED
};

// Hello
// struct Hello {
//   std::string server_name;
//   uint8_t players_count;
//   uint16_t size_x;
//   uint16_t size_y;
//   uint16_t game_length;
//   uint16_t explosion_radius;
//   uint16_t bomb_timer;
// };

using Hello =
  std::tuple<std::string, uint8_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t>;

// struct AcceptedPlayer {
//   PlayerId id;
//   Player player;
// };

using AcceptedPlayer = std::pair<PlayerId, Player>;

// also redundant?
// struct GameStarted {
//   std::map<PlayerId, Player> players;
// };

using GameStarted = std::map<PlayerId, Player>;

// struct Turn {
//   uint16_t turn;
//   std::vector<Event> events;
// };

using Turn = std::pair<uint16_t, std::vector<Event>>;

// struct GameEnded {
//   std::map<PlayerId, Score> scores;
// };

using GameEnded = std::map<PlayerId, Score>;

using ServerMessage = std::variant<Hello, AcceptedPlayer, GameStarted, Turn, GameEnded>;

}; // namespace server_messagess

namespace display_messages
{

enum DisplayMessageType {
  LOBBY, GAME
};

// struct Lobby {
//   std::string server_name;
//   uint8_t players_count;
//   uint16_t size_x;
//   uint16_t size_y;
//   uint16_t game_length;
//   uint16_t explosion_radius;
//   uint16_t bomb_timer;
//   std::map<PlayerId, server_messages::Player> players;
// };

using Lobby =
  std::tuple<std::string, uint8_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t,
             std::map<PlayerId, server_messages::Player>>;

// struct Game {
//   std::string server_name;
//   uint16_t size_x;
//   uint16_t size_y;
//   uint16_t game_length;
//   uint16_t turn;
//   std::map<PlayerId, server_messages::Player> players;
//   std::map<PlayerId, Position> player_positions;
//   std::set<Position> blocks;
//   std::set<server_messages::Bomb> bombs;
//   std::set<Position> explosions;
//   std::map<PlayerId, Score> scores;
// };

using Game =
  std::tuple<std::string, uint16_t, uint16_t, uint16_t, uint16_t,
             std::map<PlayerId, server_messages::Player>, std::map<PlayerId, Position>,
             std::set<Position>, std::set<server_messages::Bomb>, std::set<Position>,
             std::map<PlayerId, Score>>;

using DisplayMessage = std::variant<Lobby, Game>;

}; // namespace display_message

namespace input_messages
{

enum InputMessageType {
  PLACE_BOMB, PLACE_BLOCK, MOVE
};

using InputMessage = std::variant<client_messages::PlaceBomb,
                         client_messages::PlaceBlock, client_messages::Move>;

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
