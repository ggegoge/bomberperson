// This file contains first and foremost implementation of important functions
// declared at the bottom of the messages.h header -- serialisers and deserialisers
// for messages sent by the client, server and the gui. They are all based on the
// interface proposed in serialise.h with operators << and >> being the main and most
// versatile method of deserialisation in our protocole.
//
// Note how most functions here are just chained applications of the operators on
// all fields of chosen structs but unfortunately C++ does not provide a method
// to easily do this (to have some "(de)serialise struct" abstraction) and that
// is why most of the code here is boring and boilerplate.

#include "netio.h"
#include "serialise.h"
#include "messages.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <unistd.h>

namespace
{

using namespace client_messages;
using namespace server_messages;
using namespace input_messages;
using namespace display_messages;

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

// display messages
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

// deserialisation of client messages
template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Direction& d)
{
  uint8_t dir;
  deser >> dir;
  d = (Direction)dir;
  switch (dir) {
  case Up:
  case Right:
  case Down:
  case Left:
    return deser;

  default:
    throw DeserProtocolError("Invalid direction!");
  }
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct Join& j)
{
  return deser >> j.name;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct Move& mv)
{
  return deser >> mv.direction;
}

// deserialisation of various server messages
template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct Hello& hello)
{
  return deser >> hello.server_name >> hello.players_count
         >> hello.size_x >> hello.size_y >> hello.game_length
         >> hello.explosion_radius >> hello.bomb_timer;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct Player& pl)
{
  return deser >> pl.name >> pl.address;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct AcceptedPlayer& ap)
{
  return deser >> ap.id >> ap.player;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct GameStarted& gs)
{
  return deser >> gs.players;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct Turn& turn)
{
  return deser >> turn.turn >> turn.events;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct GameEnded& ge)
{
  return deser >> ge.scores;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Position& position)
{
  return deser >> position.first >> position.second;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct Bomb& b)
{
  return deser >> b.position >> b.timer;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct BombPlaced& bp)
{
  return deser >> bp.id >> bp.position;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct BombExploded& be)
{
  return deser >> be.id >> be.killed >> be.blocks_destroyed;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct PlayerMoved& pm)
{
  return deser >> pm.id >> pm.position;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, struct BlockPlaced& bp)
{
  return deser >> bp.position;
}

template <Readable R>
Deserialiser<R>& f(Deserialiser<R>& deser, EventVar& ev)
{
  uint8_t kind;
  deser >> kind;

  switch (kind) {
  case BombExploded: {
    struct BombExploded be;
    deser >> be;
    ev = be;
    return deser;
  }
  case BombPlaced: {
    struct BombPlaced bp;
    deser >> bp;
    ev = bp;
    return deser;
  }
  case PlayerMoved: {
    struct PlayerMoved pm;
    deser >> pm;
    ev = pm;
    return deser;

  }
  case BlockPlaced: {
    struct BlockPlaced bp;
    deser >> bp;
    ev = bp;
    return deser;
  }
  default: {
    throw DeserProtocolError("Wrong event type!");
  }
  }

  return deser;
}

}; // namespace anonymous

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, client_messages::ClientMessage& msg)
{
  uint8_t kind;
  deser >> kind;

  switch (kind) {
  case client_messages::Join: {
    struct Join j;
    deser >> j;
    msg = j;
    return deser;
  }
  case client_messages::PlaceBomb: {
    struct PlaceBomb pb;
    msg = pb;
    return deser;
  }
  case client_messages::PlaceBlock: {
    struct PlaceBlock pb;
    msg = pb;
    return deser;
  }
  case client_messages::Move: {
    struct Move m;
    deser >> m;
    msg = m;
    return deser;
  }
  default:
    throw DeserProtocolError("Wrong type of client message!");
  }
}


template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, server_messages::ServerMessage& msg)
{
  using namespace server_messages;
  uint8_t kind;
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
    struct AcceptedPlayer ap;
    deser >> ap;
    msg = ap;
    return deser;
  }
  case GameStarted: {
    struct GameStarted gs;
    deser >> gs;
    msg = gs;
    return deser;
  }
  case Turn: {
    struct Turn turn;
    deser >> turn;
    msg = turn;
    return deser;
  }
  case GameEnded: {
    struct GameEnded ge;
    deser >> ge;
    msg = ge;
    return deser;
  }
  default: {
    throw DeserProtocolError("wrong server message type!");
  }
  };

  return deser;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, input_messages::InputMessage& msg)
{
  uint8_t kind;
  deser >> kind;
  
  switch (kind) {
  case input_messages::PlaceBomb: {
    struct PlaceBomb pb;
    msg = pb;
    return deser;
  }
  case input_messages::PlaceBlock: {
    struct PlaceBlock pb;
    msg = pb;
    return deser;
  }
  case input_messages::Move: {
    struct Move mv;
    deser >> mv;
    msg = mv;
    return deser;
  }
  default:
    throw DeserProtocolError("Wrong client message type!");
  };
}

// Needed for separating definition from declaration.
// https://isocpp.org/wiki/faq/templates#separate-template-fn-defn-from-decl
template Deserialiser<ReaderUDP>& operator>>(Deserialiser<ReaderUDP>&, ServerMessage&);
template Deserialiser<ReaderUDP>& operator>>(Deserialiser<ReaderUDP>&, ClientMessage&);
template Deserialiser<ReaderUDP>& operator>>(Deserialiser<ReaderUDP>&, InputMessage&);
