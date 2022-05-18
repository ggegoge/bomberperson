// Messages sent in our protocol and (de)serialisers for them.
//
// I do not use structs or classes but tuples, pairs and aliases on purpose:
// with marshal.h you can marshal and unmarshal tuples etc without writing any
// extra adapters whereas if structs were to be used then it would be necessary
// to write another overloads for each one of them.

// Messages are in respective namespaces to avoid overt confusion with naming.

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <variant>
#include <map>
#include <set>
#include <tuple>
#include <string>
#include <vector>

#include "marshal.h"

using PlayerId = uint8_t;
using BombId = uint32_t;
using Position = std::pair<uint16_t, uint16_t>;
using Score = uint32_t;

namespace client_messages
{

enum Direction {
  UP, RIGHT, DOWN, LEFT,
};

// Our marshalling module does not provide a generlised way of safe (ie range checked)
// enum deserialsation.
template <Readable R>
inline Deserialiser<R>& operator>>(Deserialiser<R>& deser, Direction& d)
{
  uint8_t dir;
  deser >> dir;
  d = static_cast<Direction>(dir);
  switch (dir) {
  case UP:
  case RIGHT:
  case DOWN:
  case LEFT:
    return deser;

  default:
    throw UnmarshallingError{"Invalid direction!"};
  }
}

// Join(name)
using Join = std::string;

// Placeholders.
struct PlaceBomb {};
struct PlaceBlock {};

// Adapters for (un)marshalling to work on those empty structs.
inline Serialiser& operator<<(Serialiser& ser, const PlaceBlock&) { return ser; }
inline Serialiser& operator<<(Serialiser& ser, const PlaceBomb&) { return ser; }
template <Readable R>
inline Deserialiser<R>& operator>>(Deserialiser<R>& deser, PlaceBlock&) { return deser; }
template <Readable R>
inline Deserialiser<R>& operator>>(Deserialiser<R>& deser, PlaceBomb&) { return deser; }


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

// Lobby(serv_name, players_count, size_x, size_y, game_length, radius, timer, players)
using Lobby =
  std::tuple<std::string, uint8_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t,
             std::map<PlayerId, server_messages::Player>>;

// Game(serv_name, size_x, size_y, game_length, turn, players, player_positions,
//      blocks, bombs, explosions, scores)
using Game =
  std::tuple<std::string, uint16_t, uint16_t, uint16_t, uint16_t,
             std::map<PlayerId, server_messages::Player>, std::map<PlayerId, Position>,
             std::set<Position>, std::set<server_messages::Bomb>, std::set<Position>,
             std::map<PlayerId, Score>>;

using DisplayMessage = std::variant<Lobby, Game>;

}; // namespace display_message

namespace input_messages
{

using InputMessage = std::variant<client_messages::PlaceBomb,
                         client_messages::PlaceBlock, client_messages::Move>;

}; // namespace input_messages

#endif  // _MESSAGES_H_
