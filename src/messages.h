// Messages sent in our protocol.

// I barely use structs (or classes) -- trying to stick to tuples, pairs and aliases
// whenever I can but very much on purpose: with marshal.h you can marshal and
// unmarshal pairs, tuples etc without writing any extra adapters whereas if structs
// were to be used then it would be necessary to write overloads for each one of them.
// In fact I do it for display messages but I generally avoid that.

// Messages are in respective namespaces to avoid overt confusion with naming.

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <cstdint>
#include <utility>
#include <map>
#include <set>
#include <tuple>
#include <string>
#include <vector>
#include <variant>

#include "marshal.h"

// So common we do not have a namespace for them.
using PlayerId = uint8_t;
using BombId = uint32_t;
using Position = std::pair<uint16_t, uint16_t>;
using Score = uint32_t;

namespace client_messages
{

// Consistently using std::variant for all enum like messages essentially. Hence
// I need those empty structs for type safety.
struct Up {};
struct Right {};
struct Down {};
struct Left {};

using Direction = std::variant<Up, Right, Down, Left>;

// Join(name)
using Join = std::string;

// Placeholders.
struct PlaceBomb {};
struct PlaceBlock {};

// Move(direction)
using Move = Direction;

using ClientMessage = std::variant<Join, PlaceBomb, PlaceBlock, Move>;

}; // namespace client_message

namespace server_messages
{

// Player(name, address)
using Player = std::pair<std::string, std::string>;

// Bomb(position, timer)
using Bomb = std::pair<Position, uint16_t>;

// BombPlaced(id, position)
using BombPlaced = std::pair<BombId, Position>;

// BombExploded(id, killed, destroyed)
using BombExploded = std::tuple<BombId, std::set<PlayerId>, std::set<Position>>;

// BlockPlaced(postion)
using BlockPlaced = Position;

// PlayerMoved(id, position)
using PlayerMoved = std::pair<PlayerId, Position>;

using Event = std::variant<BombPlaced, BombExploded, PlayerMoved, BlockPlaced>;

// Hello(serv_name, players_count, size_x, size_y, game_length, radius, timer)
using Hello =
  std::tuple<std::string, uint8_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t>;

// AcceptedPlayer(id, player)
using AcceptedPlayer = std::pair<PlayerId, Player>;

// GameStarted(players)
using GameStarted = std::map<PlayerId, Player>;

// Turn(turnno, events)
using Turn = std::pair<uint16_t, std::vector<Event>>;

// GameEnded(scores)
using GameEnded = std::map<PlayerId, Score>;

using ServerMessage = std::variant<Hello, AcceptedPlayer, GameStarted, Turn, GameEnded>;

}; // namespace server_messagess

namespace display_messages
{

// Representing Game and Lobby as proper structs for the sake of client being
// able to easily modify these two. It would be painful to use tuples this big.
struct Lobby {
  std::string server_name;
  uint8_t players_count;
  uint16_t size_x;
  uint16_t size_y;
  uint16_t game_length;
  uint16_t radius;
  uint16_t timer;
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
  std::vector<server_messages::Bomb> bombs;
  std::set<Position> explosions;
  std::map<PlayerId, Score> scores;
};

// We provide adapters for serialisation of custom structs as per marshal.h
// description of the serialisation operator<< standard. No need for deserialiser
// as we only send Game and Lobby.
inline Serialiser& operator<<(Serialiser& ser, const Lobby& l)
{
  return ser << l.server_name << l.players_count << l.size_x << l.size_y
             << l.game_length << l.radius << l.timer << l.players;
}

inline Serialiser& operator<<(Serialiser& ser, const Game& g)
{
  return ser << g.server_name << g.size_x << g.size_y << g.game_length << g.turn
             << g.players << g.player_positions << g.blocks << g.bombs
             << g.explosions << g.scores;
}

using DisplayMessage = std::variant<Lobby, Game>;

}; // namespace display_message

namespace input_messages
{

using InputMessage = std::variant<client_messages::PlaceBomb,
                         client_messages::PlaceBlock, client_messages::Move>;

}; // namespace input_messages

#endif  // _MESSAGES_H_
