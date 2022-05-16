#include <cstdint>
#include <iostream>
#include <ostream>
#include <variant>
#include "messages.h"

template<typename ... Args>
void print(std::ostream& os, Args ... args)
{
    ((os << args << '\n'), ...);
}

inline std::ostream& operator<<(std::ostream& os, const struct server_messages::Hello& h)
{
  print(os, h.server_name, (int)h.players_count, h.size_x, h.size_y, h.game_length,
        h.explosion_radius, h.bomb_timer);
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const server_messages::ServerMessage& msg)
{
  uint8_t index = msg.index();
  return std::visit([&os, index] <typename T> (const T & x) -> std::ostream& {
    return os << (uint8_t)index << x;
  }, msg);
}
