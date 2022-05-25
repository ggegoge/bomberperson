// Client for the bomberperson game.

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/resolver_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/program_options.hpp>
#include <map>
#include <set>
#include <thread>
#include <tuple>
#include <type_traits>
#include <regex>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "readers.h"
#include "marshal.h"
#include "messages.h"
#include "dbg.h"

namespace po = boost::program_options;

using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::ip::resolver_base;

using input_messages::InputMessage;
using display_messages::DisplayMessage;
using server_messages::ServerMessage;
using client_messages::ClientMessage;

namespace
{

template <typename T>
concept LobbyOrGame = std::same_as<T, display_messages::Lobby> ||
  std::same_as<T, display_messages::Game>;

class ClientError : public std::runtime_error {
public:
  ClientError() : runtime_error{"Client error!"} {}
  ClientError(const std::string& msg) : runtime_error{msg} {}
};

// Helper for std::visiting mimicking pattern matching, inspired by cppref.
template<typename> inline constexpr bool always_false_v = false;

std::pair<std::string, std::string> get_addr(const std::string& addr)
{
  static const std::regex r("^(.*):(\\d+)$");
  std::smatch sm;

  if (std::regex_search(addr, sm, r)) {
    std::string ip = sm[1].str();

    // Allow IPv6 adresses like [::1] which boost's resolver does not resolve.
    if (ip.at(0) == '[') {
      ip = ip.substr(1, ip.length() - 2);
    }

    std::string port = sm[2].str();
    return {ip, port};
  } else {
    throw ClientError{"Invalid address!"};
  }
}

// Structure representing current state of affairs.
struct GameState {
  DisplayMessage state;
  std::map<BombId, server_messages::Bomb> bombs;

  // Set since "you only die once".
  std::set<PlayerId> killed_this_turn;

  // Need to keep those for proper display of explosions.
  std::set<Position> old_blocks;

  // Whether to treat gui input as player action or as a Join request.
  bool lobby = true;

  // This indicated whether the game has just started.
  bool started = false;
  // Server parameters.
  uint16_t timer;
  uint8_t players_count;
  uint16_t explosion_radius;
};

// Main class representing the client.
class RoboticClient {
  boost::asio::io_context io_ctx;
  std::string name;
  tcp::socket server_socket;
  udp::socket gui_socket;
  udp::endpoint gui_endpoint;
  tcp::endpoint server_endpoint;
  Serialiser server_ser;
  Serialiser gui_ser;
  Deserialiser<ReaderTCP> server_deser;
  Deserialiser<ReaderUDP> gui_deser;
  GameState game_state;
public:
  RoboticClient(const std::string& name, uint16_t port,
                const std::string& server_addr, const std::string& gui_addr)
    : name{name}, server_socket{io_ctx}, gui_socket{io_ctx, udp::endpoint{udp::v6(), port}},
      gui_endpoint{}, server_endpoint{}, server_deser{server_socket}, gui_deser{}
  {
    auto [gui_ip, gui_port] = get_addr(gui_addr);
    udp::resolver udp_resolver{io_ctx};
    gui_endpoint = *udp_resolver.resolve(gui_ip, gui_port, resolver_base::numeric_service);

    std::cout << "Client \"" << name << "\" communicating with endpoints:\n";

    if (gui_endpoint.protocol() == udp::v6())
      std::cout << "\tgui: [" << gui_endpoint.address() << "]:"
                << gui_endpoint.port() << "\n";
    else
      std::cout << "\tgui: " << gui_endpoint.address() << ":"
                << gui_endpoint.port() << "\n";

    auto [serv_ip, serv_port] = get_addr(server_addr);
    tcp::resolver tcp_resolver{io_ctx};
    server_endpoint =
      *tcp_resolver.resolve(serv_ip, serv_port, resolver_base::numeric_service);

    if (server_endpoint.protocol() == tcp::v6())
      std::cout << "\tserver: [" << server_endpoint.address() << "]:"
                << server_endpoint.port() << "\n";
    else
      std::cout << "\tserver: " << server_endpoint.address() << ":"
                << server_endpoint.port() << "\n";

    // Open connection to the server.
    server_socket.connect(server_endpoint);
    tcp::no_delay option(true);
    server_socket.set_option(option);
  }

  // Main function for actually playing the game.
  void play();
  
private:
  // This function will be executed by one of the threads to receive input form
  // the gui and send it forward to the server.
  void input_handler();

  // Another thread reads messages from the server and updates the game state by
  // aggregating all of the information received from the server. Then after each
  // such update it should tell the gui to show what is going on appropriately.
  void game_handler();

  // Game'ise the lobby, convets the held game _state.state.
  void lobby_to_game();

  // Fill bombs in current game_state.state based on game_state.bombs and update
  // scores of players from the killed set.
  void update_game();

  // Events affect the game in one way or another.
  void apply_event(display_messages::Game& game,
                   std::map<BombId, server_messages::Bomb>& bombs,
                   const server_messages::Event& event);

  // Variant to variant conversion (type safety), handles input messages.
  ClientMessage input_to_client(InputMessage& msg);
  
  // Handliing of messages from the server and case specific handlers.
  void server_msg_handler(ServerMessage& msg);

  void hello_handler(server_messages::Hello& hello);
  void ap_handler(server_messages::AcceptedPlayer& ap);
  void gs_handler(server_messages::GameStarted& gs);
  void turn_handler(server_messages::Turn& turn);
  void ge_handler(server_messages::GameEnded& ge);

  // For explosions.
  Position do_move(Position pos, client_messages::Direction dir) const;
  void explosions_in_radius(std::set<Position>& explosions, Position pos) const;
};

void RoboticClient::hello_handler(server_messages::Hello& h)
{
  dbg("[game_handler] hello_handler");
  using namespace display_messages;
  auto& [server_name, players_count, size_x, size_y,
         game_length, explosion_radius, bomb_timer] = h;
  dbg("[hello_handler] Hello from \"", server_name, "\".");

  Lobby l{server_name, players_count, size_x, size_y, game_length,
    explosion_radius, bomb_timer, {}};

  game_state.timer = bomb_timer;
  game_state.explosion_radius = explosion_radius;
  game_state.state = l;
  game_state.players_count = players_count;
}

void RoboticClient::ap_handler(server_messages::AcceptedPlayer& ap)
{
  using namespace display_messages;
  std::visit([&ap] <typename GorL> (GorL& gl) {
      auto& [id, player] = ap;
      dbg("[game_handler]: New player ", player.first, "@", player.second);
      gl.players.insert({id, player});
    }, game_state.state);
}

void RoboticClient::lobby_to_game()
{
  using namespace display_messages;
  game_state.state = std::visit([] <typename GorL> (GorL& gl) {
      if constexpr (std::same_as<Lobby, GorL>) {
        std::map<PlayerId, Score> scores;
        for (auto& [plid, _] : gl.players)
          scores.insert({plid, 0});

        Game g{gl.server_name, gl.size_x, gl.size_y, gl.game_length,
          0, gl.players, {}, {}, {}, {}, scores};
        return DisplayMessage{g};
      } else if constexpr (std::same_as<Game, GorL>) {
        return DisplayMessage{gl};
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);
}

void RoboticClient::gs_handler(server_messages::GameStarted& gs)
{
  dbg("[game_handler] gs_handler");
  using namespace display_messages;

  game_state.started = true;
  std::visit([&gs] <typename GorL> (GorL& gl) {
      gl.players = gs;
    }, game_state.state);

  lobby_to_game();
}

void RoboticClient::explosions_in_radius(std::set<Position>& explosions,
                                         Position bombpos) const
{
  client_messages::Direction dirs[] = {client_messages::Up{},
    client_messages::Down{}, client_messages::Left{}, client_messages::Right{}};

  // find those who have got their lives ended
  for (client_messages::Direction d : dirs) {
    Position pos = bombpos;
    // Note: <= radius as the bomb position itself is also affected.
    for (uint16_t i = 0; i <= game_state.explosion_radius; ++i) {
      Position next = do_move(pos, d);
      explosions.insert(pos);

      if (next == pos || game_state.old_blocks.contains(pos))
        break;

      pos = next;
    }
  }
}

Position RoboticClient::do_move(Position pos,
                                client_messages::Direction dir) const
{
  using namespace client_messages;
  return std::visit([this, pos] <typename D> (D) {
      auto [size_x, size_y] =
        std::visit([] <typename T> (const T& gl) -> std::pair<uint16_t, uint16_t> {
            return {gl.size_x, gl.size_y};
        }, game_state.state);
      
      auto [x, y] = pos;

      if constexpr (std::same_as<D, Up>) {
        return (y + 1 < size_y) ? Position{x, y + 1} : pos;
      } else if constexpr (std::same_as<D, Down>) {
        return (y > 0) ? Position{x, y - 1} : pos;
      } else if constexpr (std::same_as<D, Left>) {
        return (x > 0) ? Position{x - 1, y} : pos;
      } else if constexpr (std::same_as<D, Right>) {
        return (x + 1 < size_x) ? Position{x + 1, y} : pos;
      } else {
        static_assert(always_false_v<D>, "Non-exhaustive pattern matching!");
      }
    }, dir);
}

void RoboticClient::apply_event(display_messages::Game& game,
                                std::map<BombId, server_messages::Bomb>& bombs,
                                const server_messages::Event& event)
{
  using namespace server_messages;
  std::visit([&game, &bombs, this] <typename Ev> (const Ev& ev) {
      if constexpr (std::same_as<BombPlaced, Ev>) {
        auto& [id, position] = ev;
        bombs.insert({id, {position, game_state.timer}});
      } else if constexpr (std::same_as<BombExploded, Ev>) {
        auto& [id, killed, blocks_destroyed] = ev;
        explosions_in_radius(game.explosions, bombs.at(id).first);
        game.explosions.insert(bombs.at(id).first);
        bombs.erase(id);

        for (PlayerId plid : killed)
          game_state.killed_this_turn.insert(plid);

        for (Position pos : blocks_destroyed) {
          game.blocks.erase(pos);
          game.explosions.insert(pos);
        }
      } else if constexpr (std::same_as<PlayerMoved, Ev>) {
        auto& [id, position] = ev;
        game.player_positions[id] = position;
      } else if constexpr (std::same_as<BlockPlaced, Ev>) {
        game.blocks.insert(ev);
      } else {
        static_assert(always_false_v<Ev>, "Non-exhaustive pattern matching!");
      }
    }, event);
}

void RoboticClient::turn_handler(server_messages::Turn& turn)
{
  auto& [turnno, events] = turn;
  dbg("[game_handler] turn_handler, turn ", turnno);
  lobby_to_game();
  display_messages::Game& current_game =
    get<display_messages::Game>(game_state.state);

  current_game.turn = turnno;
  current_game.explosions = {};
  game_state.old_blocks = current_game.blocks;

  // Upon each turn the bombs get their timers reduced.
  for (auto& [_, bomb] : game_state.bombs)
    --bomb.second;

  for (const server_messages::Event& ev : events) {
    apply_event(current_game, game_state.bombs, ev);
  }

  // Do not show past explosions.
  if (turnno == 0)
    current_game.explosions = {};
}

void RoboticClient::ge_handler(server_messages::GameEnded& ge)
{
  dbg("[game_handler] ge_handler");
  using namespace display_messages;
  // The lobby flag gets concurrently modified but we are fine with that.
  game_state.lobby = true;
  game_state.bombs = {};
  game_state.old_blocks = {};

  const std::map<PlayerId, server_messages::Player>& players =
    std::visit([] <typename GorL> (const GorL& gl) {
      if constexpr (std::same_as<Lobby, GorL> || std::same_as<Game, GorL>) {
        return gl.players;
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);

  std::cout << "GAME ENDED!!!\n";
  for (auto [id, score] : ge)
    std::cout << static_cast<int>(id) << "\t" << players.at(id).first
         << "@" << players.at(id).second << " got killed " << score << " times!\n";

  // Generate new lobby based on what we know already.
  game_state.state = std::visit([this] <typename GorL> (GorL& gl) {
      if constexpr (std::same_as<Lobby, GorL>) {
        return DisplayMessage{gl};
      } else if constexpr (std::same_as<Game, GorL>) {
        Lobby l{gl.server_name, game_state.players_count, gl.size_x,
          gl.size_y, gl.game_length, game_state.explosion_radius, game_state.timer, {}};
        return DisplayMessage{l};
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);
}

void RoboticClient::update_game()
{
  using namespace display_messages;

  std::visit([this] <typename GorL> (GorL& gl) {
      if constexpr (std::same_as<Lobby, GorL>) {
        // No bombs in the lobby.
      } else if constexpr (std::same_as<Game, GorL>) {
        gl.bombs = {};

        for (auto& [_, bomb] : game_state.bombs)
          gl.bombs.push_back(bomb);

        for (PlayerId plid : game_state.killed_this_turn)
          ++gl.scores[plid];

        game_state.killed_this_turn = {};
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);
}

// Implementation of the server message handlers. Sadly there's no pattern
// matching in C++ (yet, we were born too early) so std::visit must do.
void RoboticClient::server_msg_handler(ServerMessage& msg)
{
  using namespace server_messages;
  std::visit([this] <typename T> (T& x) {
      if constexpr (std::same_as<T, Hello>)
        hello_handler(x);
      else if constexpr (std::same_as<T, AcceptedPlayer>)
        ap_handler(x);
      else if constexpr (std::same_as<T, GameStarted>)
        gs_handler(x);
      else if constexpr (std::same_as<T, Turn>)
        turn_handler(x);
      else if constexpr (std::same_as<T, GameEnded>)
        ge_handler(x);
      else
        static_assert(always_false_v<T>, "Non-exhaustive pattern matching!");
    }, msg);
}

ClientMessage RoboticClient::input_to_client(InputMessage& msg)
{
  using namespace client_messages;
  return std::visit([] <typename T> (T& x) -> ClientMessage {
      return x;
    }, msg);
}

void RoboticClient::input_handler()
{
  using namespace client_messages;

  ClientMessage msg;
  InputMessage inp;

  for (;;) {
    dbg("[input_handler] Waiting for input...");
    gui_deser.readable().sock_fill(gui_socket);

    try {
      gui_deser >> inp;
      gui_deser.no_trailing_bytes();
    } catch (UnmarshallingError& e) {
      dbg("[input_handler] invalid input (ignored): ", e.what());
      continue;
    }

    if (game_state.lobby) {
      dbg("[input_handler] First input in the lobby, sending Join.");
      game_state.lobby = false;
      msg = Join{name};
    } else {
      msg = input_to_client(inp);
    }

    server_ser << msg;
    dbg("[input_handler] Sending ", server_ser.size(), " bytes to the server");
    server_socket.send(boost::asio::buffer(server_ser.drain_bytes()));
  }
}

void RoboticClient::game_handler()
{
  ServerMessage updt;
  DisplayMessage msg;

  for (;;) {
    dbg("[game_handler] Tying to read a message from server...");
    server_deser >> updt;
    dbg("[game_handler] Message read, proceeding to handle it!");
    game_state.started = false;
    server_msg_handler(updt);

    update_game();

    // Apparently we should not send anything to gui after GameStarted.
    if (!game_state.started) {
      gui_ser << game_state.state;
      dbg("[game_handler] Sending ", gui_ser.size(), " bytes to gui.");
      gui_socket.send_to(boost::asio::buffer(gui_ser.drain_bytes()), gui_endpoint);
    }
  }
}

void RoboticClient::play()
{
  std::jthread game_worker{[this] { game_handler(); }};
  // Why waste the main thread, input_handler can have it.
  input_handler();
}

};  // namespace anonymous

int main(int argc, char* argv[])
{
  try {
    uint16_t portnum;
    std::string gui_addr;
    std::string player_name;
    std::string server_addr;
    po::options_description desc{"Allowed flags for the robotic client"};
    desc.add_options()
      ("help,h", "produce this help message")
      ("gui-address,d", po::value<std::string>(&gui_addr)->required(),
       "gui address, IPv4:port or IPv6:port or hostname:port")
      ("player-name,n", po::value<std::string>(&player_name)->required(), "player name")
      ("server-address,s", po::value<std::string>(&server_addr)->required(),
       "server address, same format as gui address")
      ("port,p", po::value<uint16_t>(&portnum)->required(),
       "listen to gui on a port.")
    ;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
              options(desc).run(), vm);

    std::cout << "\t\tBOMBERPERSON\n";

    if (vm.count("help")) {
      std::cout << "Usage: " << argv[0] <<  " [flags]\n";
      std::cout << desc;
      return 0;
    }

    // Notify about missing options only after printing help.
    po::notify(vm);

    RoboticClient client{player_name, portnum, server_addr, gui_addr};
    client.play();

  } catch (po::required_option& e) {
    std::cerr << "Missing some options: " << e.what() << "\n";
    std::cerr << "See " << argv[0] << " -h for help.\n";
    return 1;
  } catch (ClientError& e) {
    std::cerr << "Client error: " << e.what() << "\n";
  } catch (std::exception& e) {
    std::cerr << "Other exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}

