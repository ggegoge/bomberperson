#include <cstdint>
#include <iostream>
#include <ostream>
#include <variant>
#include "messages.h"

template<typename ... Args>
void print(std::ostream& os, Args ... args)
{
    ((os << args << ' '), ...);
}

inline std::ostream& operator<<(std::ostream& os, const struct server_messages::GameEnded&)
{
  return os << "GameEnded[]";
}

inline std::ostream& operator<<(std::ostream& os, const struct server_messages::Turn& t)
{
  return os << "Turn[" << t.turn << "]";
}

inline std::ostream& operator<<(std::ostream& os, const struct server_messages::GameStarted&)
{
  return os << "GameStarted[]";
}

inline std::ostream& operator<<(std::ostream& os, const struct server_messages::AcceptedPlayer& ap)
{
  return os << "AcceptedPlayer[" << ap.id << ap.player.name << "]";
}

inline std::ostream& operator<<(std::ostream& os, const struct server_messages::Hello& h)
{
  os << "Hello[";
  print(os, h.server_name, (int)h.players_count, h.size_x, h.size_y, h.game_length,
        h.explosion_radius, h.bomb_timer);
  
  return os << "]";
}

inline std::ostream& operator<<(std::ostream& os, const server_messages::ServerMessage& msg)
{
  std::cerr << "msg jest printeed\n";
  uint8_t index = msg.index();
  return std::visit([&os, index] <typename T> (const T & x) -> std::ostream& {
      return os << "[" << (int)index << "]" << x;
  }, msg);
}
