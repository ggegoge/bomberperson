#include "messages.h"

using namespace client_messages;
using namespace server_messages;

// Overloading operator>> where needed.

Ser& operator<<(Ser& ser, const struct Join& j)
{
  return ser << Join << j.name;
}

Ser& operator<<(Ser& ser, const struct Move& m)
{
  return ser << Move << m.direction;
}

// server messages
Ser& operator<<(Ser& ser, const struct Hello& hello)
{
  return ser << Hello << hello.server_name << hello.players_count
         << hello.size_x << hello.size_y << hello.game_length
         << hello.explosion_radius << hello.bomb_timer;
}

Ser& operator<<(Ser& ser, const struct AcceptedPlayer& ap)
{
  return ser << AcceptedPlayer << ap.id << ap.player;
}

Ser& operator<<(Ser& ser, const struct GameStarted& gs)
{
  return ser << GameStarted << gs.players;
}

Ser& operator<<(Ser& ser, const struct Turn& turn)
{
  return ser << Turn << turn.turn << turn.events;
}

Ser& operator<<(Ser& ser, const struct GameEnded& ge)
{
  return ser << GameEnded << ge.scores;
}

Ser& operator<<(Ser& ser, const Position& position)
{
  return ser << position.first << position.second;
}

Ser& operator<<(Ser& ser, const struct BombPlaced& bp)
{
  return ser << BombPlaced << bp.id << bp.position;
}

Ser& operator<<(Ser& ser, const struct BombExploded& be)
{
  return ser << BombExploded << be.id
         << be.robots_destroyed << be.blocks_destroyed;
}

Ser& operator<<(Ser& ser, const struct PlayerMoved& pm)
{
  return ser << PlayerMoved << pm.id << pm.position;
}

Ser& operator<<(Ser& ser, const struct BlockPlaced& bp)
{
  return ser << BlockPlaced << bp.position;
}

Ser& operator<<(Ser& ser, const EventVar& ev)
{
  return std::visit([&ser] <typename T> (const T & x) -> Ser& {
    return ser << x;
  }, ev);
}
