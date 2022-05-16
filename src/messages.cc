
#include "netio.h"
#include "serialise.h"
#include "messages.h"
#include <cstdint>
#include <iostream>

namespace
{

using namespace client_messages;
using namespace server_messages;
using namespace input_messages;
using namespace display_messages;
// Overloading operator>> where needed.

Serialiser& operator<<(Serialiser& ser, const struct Join& j)
{
  return ser << j.name;
}

Serialiser& operator<<(Serialiser& ser, const struct PlaceBlock&)
{
  return ser;
}

Serialiser& operator<<(Serialiser& ser, const struct PlaceBomb&)
{
  return ser;
}

Serialiser& operator<<(Serialiser& ser, const struct Move& m)
{
  return ser << m.direction;
}

// server messages
Serialiser& operator<<(Serialiser& ser, const struct Hello& hello)
{
  return ser << hello.server_name << hello.players_count
         << hello.size_x << hello.size_y << hello.game_length
         << hello.explosion_radius << hello.bomb_timer;
}

Serialiser& operator<<(Serialiser& ser, const struct Player& pl)
{
  return ser << pl.name << pl.address;
}

Serialiser& operator<<(Serialiser& ser, const struct AcceptedPlayer& ap)
{
  return ser << ap.id << ap.player;
}

Serialiser& operator<<(Serialiser& ser, const struct GameStarted& gs)
{
  return ser << gs.players;
}

Serialiser& operator<<(Serialiser& ser, const struct Turn& turn)
{
  return ser << turn.turn << turn.events;
}

Serialiser& operator<<(Serialiser& ser, const struct GameEnded& ge)
{
  return ser << ge.scores;
}

Serialiser& operator<<(Serialiser& ser, const Position& position)
{
  return ser << position.first << position.second;
}

Serialiser& operator<<(Serialiser& ser, const struct Bomb& b)
{
  std::cerr << "bomb!\n";
  return ser << b.position << b.timer;
}

Serialiser& operator<<(Serialiser& ser, const struct BombPlaced& bp)
{
  return ser << bp.id << bp.position;
}

Serialiser& operator<<(Serialiser& ser, const struct BombExploded& be)
{
  return ser << be.id << be.killed << be.blocks_destroyed;
}

Serialiser& operator<<(Serialiser& ser, const struct PlayerMoved& pm)
{
  return ser << pm.id << pm.position;
}

Serialiser& operator<<(Serialiser& ser, const struct BlockPlaced& bp)
{
  return ser << bp.position;
}

// next three should be in the anpn namespace but then i get warnings?....
Serialiser& operator<<(Serialiser& ser, const EventVar& ev)
{
  uint8_t index = ev.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Serialiser& {
    return ser << (uint8_t)index << x;
  }, ev);
}

Serialiser& operator<<(Serialiser& ser, const struct Lobby& l)
{
  return ser << l.server_name << l.players_count << l.size_x << l.size_y <<
         l.game_length << l.explosion_radius << l.bomb_timer << l.players;
}

Serialiser& operator<<(Serialiser& ser, const struct Game& g)
{
  return ser << g.server_name << g.size_x << g.size_y << g.game_length << g.turn
             << g.players << g.player_positions << g.blocks << g.bombs
             << g.explosions << g.scores;
}

}; // namespace anonymous

Serialiser& operator<<(Serialiser& ser, const ServerMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Serialiser& {
    return ser << (uint8_t)index << x;
  }, msg);
}

Serialiser& operator<<(Serialiser& ser, const ClientMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Serialiser& {
    return ser << (uint8_t)index << x;
  }, msg);
}

Serialiser& operator<<(Serialiser& ser, const DisplayMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Serialiser& {
    return ser << (uint8_t)index << x;
  }, msg);
}

Serialiser& operator<<(Serialiser& ser, const InputMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Serialiser& {
    return ser << (uint8_t)index << x;
  }, msg);
}

// DESER
namespace {

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct Hello& hello)
{
  std::cerr << "deser >> hello\n";
  return deser >> hello.server_name >> hello.players_count
         >> hello.size_x >> hello.size_y >> hello.game_length
         >> hello.explosion_radius >> hello.bomb_timer;
}

}; // namespace anonymous

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, server_messages::ServerMessage& msg)
{
  using namespace server_messages;
  ServerMessageType kind;
  deser >> kind;

  switch (kind) {
  case Hello: {
    std::cerr << "kind -> hello\n";
    struct Hello hello;
    deser >> hello;
    msg = hello;
    return deser;
  }
  case AcceptedPlayer: {
    std::cerr << "todo\n";
    exit(1);
  }
  case GameStarted: {
    std::cerr << "todo\n";
    exit(1);    
  }
  case Turn: {
    std::cerr << "todo\n";
    exit(1);    
  }
  case GameEnded: {
    std::cerr << "todo\n";
    exit(1);    
  } 
  };

  return deser;
}

// Needed for separating definition from declaration.
// https://isocpp.org/wiki/faq/templates#separate-template-fn-defn-from-decl
template Deserialiser<ReaderUDP>& operator>>(Deserialiser<ReaderUDP>&, ServerMessage&);
template Deserialiser<ReaderTCP>& operator>>(Deserialiser<ReaderTCP>&, ServerMessage&);
