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

enum EventType {
  BOMB_PLACED, BOMB_EXPLODED, PLAYER_MOVED, BLOCK_PLACED
};

// BombPlaced(id, position)
using BombPlaced = std::pair<BombId, Position>;

// BombExploded(id, killed, destroyed)
using BombExploded = std::tuple<BombId, std::set<PlayerId>, std::set<Position>>;

// BlockPlaced(postion)
using BlockPlaced = Position;

// PlayerMoved(id, position)
using PlayerMoved = std::pair<PlayerId, Position>;

using Event = std::variant<BombPlaced, BombExploded, PlayerMoved, BlockPlaced>;

enum ServerMessageType {
  HELLO, ACCEPTED_PLAYER, GAME_STARTED, TURN, GAME_ENDED
};

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

enum DisplayMessageType {
  LOBBY, GAME
};

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
