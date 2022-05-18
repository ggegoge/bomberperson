// This file contains first and foremost implementation of important functions
// declared at the bottom of the messages.h header -- serialisers and deserialisers
// for messages sent by the client, server and the gui. They are all based on the
// interface proposed in marshal.h with operators << and >> being the main and most
// versatile method of deserialisation in our protocole.

// Note how most functions here are just chained applications of the operators on
// all fields of chosen structs. Unfortunately C++ does not provide a method
// to easily do this (to have some "(de)serialise struct" abstraction) and that
// is why most of the code here is boring and boilerplate.

#include "readers.h"
#include "marshal.h"
#include "messages.h"
#include <algorithm>
#include <cstdint>
#include <unistd.h>

using namespace client_messages;
using namespace server_messages;
using namespace input_messages;
using namespace display_messages;

namespace
{

Serialiser& operator<<(Serialiser& ser, const Join& j)
{
  return ser << j.name;
}

Serialiser& operator<<(Serialiser& ser, const PlaceBlock&)
{
  return ser;
}

Serialiser& operator<<(Serialiser& ser, const PlaceBomb&)
{
  return ser;
}

Serialiser& operator<<(Serialiser& ser, const Move& m)
{
  return ser << m.direction;
}

// server messages
Serialiser& operator<<(Serialiser& ser, const Hello& hello)
{
  return ser << hello.server_name << hello.players_count
         << hello.size_x << hello.size_y << hello.game_length
         << hello.explosion_radius << hello.bomb_timer;
}

Serialiser& operator<<(Serialiser& ser, const Player& pl)
{
  return ser << pl.name << pl.address;
}

Serialiser& operator<<(Serialiser& ser, const AcceptedPlayer& ap)
{
  return ser << ap.id << ap.player;
}

Serialiser& operator<<(Serialiser& ser, const GameStarted& gs)
{
  return ser << gs.players;
}

Serialiser& operator<<(Serialiser& ser, const Turn& turn)
{
  return ser << turn.turn << turn.events;
}

Serialiser& operator<<(Serialiser& ser, const GameEnded& ge)
{
  return ser << ge.scores;
}

Serialiser& operator<<(Serialiser& ser, const Position& position)
{
  return ser << position.first << position.second;
}

Serialiser& operator<<(Serialiser& ser, const Bomb& b)
{
  return ser << b.position << b.timer;
}

Serialiser& operator<<(Serialiser& ser, const BombPlaced& bp)
{
  return ser << bp.id << bp.position;
}

Serialiser& operator<<(Serialiser& ser, const BombExploded& be)
{
  return ser << be.id << be.killed << be.blocks_destroyed;
}

Serialiser& operator<<(Serialiser& ser, const PlayerMoved& pm)
{
  return ser << pm.id << pm.position;
}

Serialiser& operator<<(Serialiser& ser, const BlockPlaced& bp)
{
  return ser << bp.position;
}

// next three should be in the anpn namespace but then i get warnings?....
Serialiser& operator<<(Serialiser& ser, const Event& ev)
{
  uint8_t index = static_cast<uint8_t>(ev.index());
  return std::visit([&ser, index] <typename T> (const T& x) -> Serialiser& {
    return ser << index << x;
  }, ev);
}

// display messages
Serialiser& operator<<(Serialiser& ser, const Lobby& l)
{
  return ser << l.server_name << l.players_count << l.size_x << l.size_y <<
         l.game_length << l.explosion_radius << l.bomb_timer << l.players;
}

Serialiser& operator<<(Serialiser& ser, const Game& g)
{
  return ser << g.server_name << g.size_x << g.size_y << g.game_length << g.turn
             << g.players << g.player_positions << g.blocks << g.bombs
             << g.explosions << g.scores;
}

}; // namespace anonymous

Serialiser& operator<<(Serialiser& ser, const ServerMessage& msg)
{
  uint8_t index = static_cast<uint8_t>(msg.index());
  return std::visit([&ser, index] <typename T> (const T& x) -> Serialiser& {
    return ser << index << x;
  }, msg);
}

Serialiser& operator<<(Serialiser& ser, const ClientMessage& msg)
{
  uint8_t index = static_cast<uint8_t>(msg.index());
  return std::visit([&ser, index] <typename T> (const T& x) -> Serialiser& {
    return ser << index << x;
  }, msg);
}

Serialiser& operator<<(Serialiser& ser, const DisplayMessage& msg)
{
  uint8_t index = static_cast<uint8_t>(msg.index());
  return std::visit([&ser, index] <typename T> (const T& x) -> Serialiser& {
    return ser << index << x;
  }, msg);
}

Serialiser& operator<<(Serialiser& ser, const InputMessage& msg)
{
  uint8_t index = static_cast<uint8_t>(msg.index());
  return std::visit([&ser, index] <typename T> (const T& x) -> Serialiser& {
    return ser << index << x;
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
  d = static_cast<Direction>(dir);
  switch (dir) {
  case UP:
  case RIGHT:
  case DOWN:
  case LEFT:
    return deser;

  default:
    throw UnmarshallingError("Invalid direction!");
  }
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Join& j)
{
  return deser >> j.name;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Move& mv)
{
  return deser >> mv.direction;
}

// deserialisation of various server messages
template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Hello& hello)
{
  return deser >> hello.server_name >> hello.players_count
         >> hello.size_x >> hello.size_y >> hello.game_length
         >> hello.explosion_radius >> hello.bomb_timer;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Player& pl)
{
  return deser >> pl.name >> pl.address;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, AcceptedPlayer& ap)
{
  return deser >> ap.id >> ap.player;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, GameStarted& gs)
{
  return deser >> gs.players;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Turn& turn)
{
  return deser >> turn.turn >> turn.events;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, GameEnded& ge)
{
  return deser >> ge.scores;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Position& position)
{
  return deser >> position.first >> position.second;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Bomb& b)
{
  return deser >> b.position >> b.timer;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, BombPlaced& bp)
{
  return deser >> bp.id >> bp.position;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, BombExploded& be)
{
  return deser >> be.id >> be.killed >> be.blocks_destroyed;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, PlayerMoved& pm)
{
  return deser >> pm.id >> pm.position;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, BlockPlaced& bp)
{
  return deser >> bp.position;
}

template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, Event& ev)
{
  uint8_t kind;
  deser >> kind;

  switch (kind) {
  case BOMB_EXPLODED: {
    BombExploded be;
    deser >> be;
    ev = be;
    return deser;
  }
  case BOMB_PLACED: {
    BombPlaced bp;
    deser >> bp;
    ev = bp;
    return deser;
  }
  case PLAYER_MOVED: {
    PlayerMoved pm;
    deser >> pm;
    ev = pm;
    return deser;

  }
  case BLOCK_PLACED: {
    BlockPlaced bp;
    deser >> bp;
    ev = bp;
    return deser;
  }
  default: {
    throw UnmarshallingError("Wrong event type!");
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
  case client_messages::JOIN: {
    Join j;
    deser >> j;
    msg = j;
    return deser;
  }
  case client_messages::PLACE_BOMB: {
    PlaceBomb pb;
    msg = pb;
    return deser;
  }
  case client_messages::PLACE_BLOCK: {
    PlaceBlock pb;
    msg = pb;
    return deser;
  }
  case client_messages::MOVE: {
    Move m;
    deser >> m;
    msg = m;
    return deser;
  }
  default:
    throw UnmarshallingError("Wrong type of client message!");
  }
}


template <Readable R>
Deserialiser<R>& operator>>(Deserialiser<R>& deser, server_messages::ServerMessage& msg)
{
  using namespace server_messages;
  uint8_t kind;
  deser >> kind;

  switch (kind) {
  case HELLO: {
    Hello hello;
    deser >> hello;
    msg = hello;
    return deser;
  }
  case ACCEPTED_PLAYER: {
    AcceptedPlayer ap;
    deser >> ap;
    msg = ap;
    return deser;
  }
  case GAME_STARTED: {
    GameStarted gs;
    deser >> gs;
    msg = gs;
    return deser;
  }
  case TURN: {
    Turn turn;
    deser >> turn;
    msg = turn;
    return deser;
  }
  case GAME_ENDED: {
    GameEnded ge;
    deser >> ge;
    msg = ge;
    return deser;
  }
  default: {
    throw UnmarshallingError("wrong server message type!");
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
  case input_messages::PLACE_BOMB: {
    PlaceBomb pb;
    msg = pb;
    return deser;
  }
  case input_messages::PLACE_BLOCK: {
    PlaceBlock pb;
    msg = pb;
    return deser;
  }
  case input_messages::MOVE: {
    Move mv;
    deser >> mv;
    msg = mv;
    return deser;
  }
  default:
    throw UnmarshallingError("Wrong client message type!");
  };
}

// Needed for separating definition from declaration.
// https://isocpp.org/wiki/faq/templates#separate-template-fn-defn-from-decl
template Deserialiser<ReaderUDP>& operator>>(Deserialiser<ReaderUDP>&, ServerMessage&);
template Deserialiser<ReaderUDP>& operator>>(Deserialiser<ReaderUDP>&, ClientMessage&);
template Deserialiser<ReaderUDP>& operator>>(Deserialiser<ReaderUDP>&, InputMessage&);

template Deserialiser<ReaderTCP>& operator>>(Deserialiser<ReaderTCP>&, ServerMessage&);
template Deserialiser<ReaderTCP>& operator>>(Deserialiser<ReaderTCP>&, ClientMessage&);
template Deserialiser<ReaderTCP>& operator>>(Deserialiser<ReaderTCP>&, InputMessage&);
