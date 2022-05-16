
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

Ser& operator<<(Ser& ser, const struct Join& j)
{
  return ser << j.name;
}

Ser& operator<<(Ser& ser, const struct PlaceBlock&)
{
  return ser;
}

Ser& operator<<(Ser& ser, const struct PlaceBomb&)
{
  return ser;
}

Ser& operator<<(Ser& ser, const struct Move& m)
{
  return ser << m.direction;
}

// server messages
Ser& operator<<(Ser& ser, const struct Hello& hello)
{
  return ser << hello.server_name << hello.players_count
         << hello.size_x << hello.size_y << hello.game_length
         << hello.explosion_radius << hello.bomb_timer;
}

Ser& operator<<(Ser& ser, const struct Player& pl)
{
  return ser << pl.name << pl.address;
}

Ser& operator<<(Ser& ser, const struct AcceptedPlayer& ap)
{
  return ser << ap.id << ap.player;
}

Ser& operator<<(Ser& ser, const struct GameStarted& gs)
{
  return ser << gs.players;
}

Ser& operator<<(Ser& ser, const struct Turn& turn)
{
  return ser << turn.turn << turn.events;
}

Ser& operator<<(Ser& ser, const struct GameEnded& ge)
{
  return ser << ge.scores;
}

Ser& operator<<(Ser& ser, const Position& position)
{
  return ser << position.first << position.second;
}

Ser& operator<<(Ser& ser, const struct Bomb& b)
{
  std::cerr << "bomb!\n";
  return ser << b.position << b.timer;
}

Ser& operator<<(Ser& ser, const struct BombPlaced& bp)
{
  return ser << bp.id << bp.position;
}

Ser& operator<<(Ser& ser, const struct BombExploded& be)
{
  return ser << be.id << be.killed << be.blocks_destroyed;
}

Ser& operator<<(Ser& ser, const struct PlayerMoved& pm)
{
  return ser << pm.id << pm.position;
}

Ser& operator<<(Ser& ser, const struct BlockPlaced& bp)
{
  return ser << bp.position;
}

// next three should be in the anpn namespace but then i get warnings?....
Ser& operator<<(Ser& ser, const EventVar& ev)
{
  uint8_t index = ev.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Ser& {
    return ser << (uint8_t)index << x;
  }, ev);
}

Ser& operator<<(Ser& ser, const struct Lobby& l)
{
  return ser << l.server_name << l.players_count << l.size_x << l.size_y <<
         l.game_length << l.explosion_radius << l.bomb_timer << l.players;
}

Ser& operator<<(Ser& ser, const struct Game& g)
{
  return ser << g.server_name << g.size_x << g.size_y << g.game_length << g.turn
             << g.players << g.player_positions << g.blocks << g.bombs
             << g.explosions << g.scores;
}

}; // namespace anonymous

Ser& operator<<(Ser& ser, const ServerMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Ser& {
    return ser << (uint8_t)index << x;
  }, msg);
}

Ser& operator<<(Ser& ser, const ClientMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Ser& {
    return ser << (uint8_t)index << x;
  }, msg);
}

Ser& operator<<(Ser& ser, const DisplayMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Ser& {
    return ser << (uint8_t)index << x;
  }, msg);
}

Ser& operator<<(Ser& ser, const InputMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&ser, index] <typename T> (const T & x) -> Ser& {
    return ser << (uint8_t)index << x;
  }, msg);
}

// DESER
namespace {

template <Readable R>
Deser<R>& operator>>(Deser<R>& deser, struct Hello& hello)
{
  std::cerr << "deser >> hello\n";
  return deser >> hello.server_name >> hello.players_count
         >> hello.size_x >> hello.size_y >> hello.game_length
         >> hello.explosion_radius >> hello.bomb_timer;
}

}; // namespace anonymous

template <Readable R>
Deser<R>& operator>>(Deser<R>& deser, server_messages::ServerMessage& msg)
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

template Deser<ReaderUDP>& operator>>(Deser<ReaderUDP>&, ServerMessage&);
