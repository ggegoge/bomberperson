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

enum ClientMessage {
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

// Overloading operator>> where needed.

inline Ser& operator<<(Ser& ser, const struct client_messages::Join& j)
{
  return ser << client_messages::Join << j.name;
}

inline Ser& operator<<(Ser& ser, const struct client_messages::Move& m)
{
  return ser << client_messages::Move << m.direction;
}

// todo: is there point in serialising enums with one field?
namespace server_message {

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

enum ServerMessage {
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

}; // namespace server_message

// ok now, adding overloads for server_messages where needed
// this is going to be painful innit...

inline Ser& operator<<(Ser& ser, const struct server_message::Hello& hello)
{
  return ser << server_message::Hello << hello.server_name << hello.players_count
             << hello.size_x << hello.size_y << hello.game_length
             << hello.explosion_radius << hello.bomb_timer;
}

inline Ser& operator<<(Ser& ser, const struct server_message::AcceptedPlayer& ap)
{
  return ser << server_message::AcceptedPlayer << ap.id << ap.player;
}

inline Ser& operator<<(Ser& ser, const struct server_message::GameStarted& gs)
{
  return ser << server_message::GameStarted << gs.players;
}

inline Ser& operator<<(Ser& ser, const struct server_message::Turn& turn)
{
  return ser << server_message::Turn << turn.turn << turn.events;
}

inline Ser& operator<<(Ser& ser, const struct server_message::GameEnded& ge)
{
  return ser << server_message::GameEnded << ge.scores;
}

inline Ser& operator<<(Ser& ser, const server_message::Position& position)
{
  return ser << position.first << position.second;
}

// All event types may be serialised!
inline Ser& operator<<(Ser& ser, const struct server_message::BombPlaced& bp)
{
  return ser << server_message::BombPlaced << bp.id << bp.position;
}

inline Ser& operator<<(Ser& ser, const struct server_message::BombExploded& be)
{
  return ser << server_message::BombExploded << be.id
             << be.robots_destroyed << be.blocks_destroyed;
}

inline Ser& operator<<(Ser& ser, const struct server_message::PlayerMoved& pm)
{
  return ser << server_message::PlayerMoved << pm.id << pm.position;
}

inline Ser& operator<<(Ser& ser, const struct server_message::BlockPlaced& bp)
{
  return ser << server_message::BlockPlaced << bp.position;
}

inline Ser& operator<<(Ser& ser, const server_message::EventVar& ev)
{
  return std::visit([&ser] <typename T> (const T& x) -> Ser& {
      return ser << x;
    }, ev);
}

namespace gui_message {

}; // namespace gui_message

#endif  // _MESSAGES_H_
