// Implementation of a client for the robots game.

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/resolver_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <map>
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

#ifdef NDEBUG
constexpr bool debug = false;
#else
constexpr bool debug = true;
#endif  // NDEBUG

// Print a debug line to the stderr (only if NDEBUG is not defined).
template <typename... Args>
void dbg(Args&&... args)
{
  if (debug) {
    (std::cerr << ... << args);
    std::cerr << "\n";
  }
}

template <typename T>
concept LobbyOrGame = std::same_as<T, display_messages::Lobby> ||
  std::same_as<T, display_messages::Game>;

class ClientError : public std::runtime_error {
public:
  ClientError()
    : runtime_error("Client error!") {}

  ClientError(const std::string& msg) : runtime_error(msg) {}
};

// Helper for std::visiting mimicking pattern matching, inspired by cppref.
template<typename> inline constexpr bool always_false_v = false;

struct GameState {
  DisplayMessage state;
  std::map<BombId, server_messages::Bomb> bombs;
  // set since "you only die once"
  std::set<PlayerId> killed_this_turn;
  // this bool will tell us whether to ignore the gui input or not.
  bool observer = false;
  bool lobby = true;
  // data from the server
  uint16_t timer;
  uint8_t players_count;
  uint16_t explosion_radius;
};

std::pair<std::string, std::string> get_addr(const std::string& addr)
{
  static const std::regex r("^(.*):(\\d+)$");
  std::smatch sm;

  if (std::regex_search(addr, sm, r)) {
    std::string ip = sm[1].str();

    // allow IPv6 adresses like [::1]?
    if (ip.at(0) == '[') {
      ip = ip.substr(1, ip.length() - 2);
    }

    std::string port = sm[2].str();
    return {ip, port};
  } else {
    throw ClientError{"Invalid address!"};
  }
}

// Accessing fields of DisplayMessage tuples.
template <LobbyOrGame T>
std::map<PlayerId, server_messages::Player>& state_get_players(T& gl)
{
  if constexpr(std::same_as<T, display_messages::Lobby>)
    return get<7>(gl);
  else if constexpr(std::same_as<T, display_messages::Game>)
    return get<5>(gl);
}

template <LobbyOrGame T>
const std::map<PlayerId, server_messages::Player>& state_get_players(const T& gl)
{
  if constexpr(std::same_as<T, display_messages::Lobby>)
    return get<7>(gl);
  else if constexpr(std::same_as<T, display_messages::Game>)
    return get<5>(gl);
}


uint16_t& game_get_turn(display_messages::Game& game)
{
  return get<4>(game);
}

std::set<server_messages::Bomb>& game_get_bombs(display_messages::Game& game)
{
  return get<8>(game);
}

std::map<PlayerId, Score>& game_get_scores(display_messages::Game& game)
{
  return get<10>(game);
}

std::set<Position>& game_get_explosions(display_messages::Game& game)
{
  return get<9>(game);
}

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
      gui_endpoint{}, server_endpoint{}, server_deser{server_socket}, gui_deser{{}}
  {
    auto [gui_ip, gui_port] = get_addr(gui_addr);
    udp::resolver udp_resolver{io_ctx};
    gui_endpoint = *udp_resolver.resolve(gui_ip, gui_port, resolver_base::numeric_service);

    std::cerr << "Resolved adresses:\n";
    if (gui_endpoint.protocol() == udp::v6())
      std::cerr << "\tgui: [" << gui_endpoint.address() << "]:"
                << gui_endpoint.port() << "\n";
    else
      std::cerr << "\tgui: " << gui_endpoint.address() << ":"
                << gui_endpoint.port() << "\n";

    auto [serv_ip, serv_port] = get_addr(server_addr);
    tcp::resolver tcp_resolver{io_ctx};
    server_endpoint =
      *tcp_resolver.resolve(serv_ip, serv_port, resolver_base::numeric_service);

    if (server_endpoint.protocol() == tcp::v6())
      std::cerr << "\tserver: [" << server_endpoint.address() << "]:"
                << server_endpoint.port() << "\n";
    else
      std::cerr << "\tserver: " << server_endpoint.address() << ":"
                << server_endpoint.port() << "\n";

    // open connection to the server
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

  // Game'ise the lobby, changes the held game state appropriately.
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
};

void RoboticClient::hello_handler(server_messages::Hello& h)
{
  dbg("[game_handler] hello handler");
  using namespace display_messages;
  auto& [server_name, players_count, size_x, size_y,
         game_length, explosion_radius, bomb_timer] = h;
  dbg("[hello_handler] hello from ", server_name);

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
      dbg("[ap_handler]: new player ", player.first, "@", player.second);
      state_get_players(gl).insert({id, player});
    }, game_state.state);
}

void RoboticClient::lobby_to_game()
{
  using namespace display_messages;
  game_state.state = std::visit([] <typename GorL> (GorL& gl) {
      if constexpr(std::same_as<Lobby, GorL>) {
        std::map<PlayerId, Score> scores;
        for (auto& [plid, _] : state_get_players(gl))
          scores.insert({plid, 0});

        auto& [server_name, players_count, size_x, size_y,
               game_length, explosion_radius, timer, players] = gl;

        Game g{server_name, size_x, size_y, game_length,
          0, players, {}, {}, {}, {}, scores};
        return DisplayMessage{g};
      } else if constexpr(std::same_as<Game, GorL>) {
        return DisplayMessage{gl};
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);
}

// do we get send this message only when we joined a game that 
void RoboticClient::gs_handler(server_messages::GameStarted& gs)
{
  dbg("[game_handler] gs_handler");
  using namespace display_messages;
  game_state.observer = true;

  std::visit([&gs] <typename GorL> (GorL& gl) {
      state_get_players(gl) = gs;
    }, game_state.state);

  lobby_to_game();
}

void RoboticClient::apply_event(display_messages::Game& game,
                                std::map<BombId, server_messages::Bomb>& bombs,
                                const server_messages::Event& event)
{
  using namespace server_messages;
  std::visit([&game, &bombs, this] <typename Ev> (const Ev& ev) {
      auto& [_1, _2, _3, _4, _5, _6, player_positions, blocks, _9, explosions, _11] = game;

      if constexpr(std::same_as<BombPlaced, Ev>) {
        auto& [id, position] = ev;
        bombs.insert({id, {position, game_state.timer}});
      } else if constexpr(std::same_as<BombExploded, Ev>) {
        auto& [id, killed, blocks_destroyed] = ev;
        explosions.insert(bombs.at(id).first);
        bombs.erase(id);

        for (PlayerId plid : killed)
          game_state.killed_this_turn.insert(plid);

        for (Position pos : blocks_destroyed)
          blocks.erase(pos);
      } else if constexpr(std::same_as<PlayerMoved, Ev>) {
        auto& [id, position] = ev;
        player_positions[id] = position;
      } else if constexpr(std::same_as<BlockPlaced, Ev>) {
        blocks.insert(ev);
      } else {
        static_assert(always_false_v<Ev>, "Non-exhaustive pattern matching!");
      }
    }, event);
}

void RoboticClient::turn_handler(server_messages::Turn& turn)
{
  auto& [turnno, events] = turn;
  dbg("[game_handler] turn handler, turn=", turnno);
  lobby_to_game();
  display_messages::Game& current_game =
    get<display_messages::Game>(game_state.state);

  game_get_turn(current_game) = turnno;
  game_get_explosions(current_game) = {};

  for (const server_messages::Event& ev : events) {
    apply_event(current_game, game_state.bombs, ev);
  }

  // upon each turn the bombs get their timers reduced.
  for (auto& [_, bomb] : game_state.bombs)
    --bomb.second;
}

void RoboticClient::ge_handler(server_messages::GameEnded& ge)
{
  dbg("[game_handler] ge_handler");
  using namespace display_messages;
  // note: we do not care about races towards "lobby"
  game_state.lobby = true;
  game_state.bombs = {};

  const std::map<PlayerId, server_messages::Player>& players =
    std::visit([] <typename GorL> (const GorL& gl) {
      if constexpr(std::same_as<Lobby, GorL> || std::same_as<Game, GorL>) {
        return state_get_players(gl);
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);

  std::cout << "GAME ENDED!!!\n";

  // todo: add assertions for this score matching the aggregated scores
  for (auto [id, score] : ge)
    std::cout << static_cast<int>(id) << "\t" << players.at(id).first
         << "@" << players.at(id).second << " got killed " << score << " times!\n";

  // new lobby based on what we know
  game_state.state = std::visit([this] <typename GorL> (GorL& gl) {
      if constexpr(std::same_as<Lobby, GorL>) {
        return DisplayMessage{gl};
      } else if constexpr(std::same_as<Game, GorL>) {
        auto& [server_name, size_x, size_y, game_length,
               _5, _6, _7, _8, _9, _10, _11] = gl;

        Lobby l{server_name, game_state.players_count, size_x,
          size_y, game_length, game_state.explosion_radius, game_state.timer, {}};
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
      if constexpr(std::same_as<Lobby, GorL>) {
        // No bombs in the lobby.
        return;
      } else if constexpr(std::same_as<Game, GorL>) {
        game_get_bombs(gl) = {};

        for (auto& [_, bomb] : game_state.bombs)
          game_get_bombs(gl).insert(bomb);

        for (PlayerId plid : game_state.killed_this_turn)
          ++game_get_scores(gl)[plid];

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
      if constexpr(std::same_as<T, Hello>)
        hello_handler(x);
      else if constexpr(std::same_as<T, AcceptedPlayer>)
        ap_handler(x);
      else if constexpr(std::same_as<T, GameStarted>)
        gs_handler(x);
      else if constexpr(std::same_as<T, Turn>)
        turn_handler(x);
      else if constexpr(std::same_as<T, GameEnded>)
        ge_handler(x);
      else
        static_assert(always_false_v<T>, "Non-exhaustive pattern matching!");
    }, msg);
}

ClientMessage RoboticClient::input_to_client(InputMessage& msg)
{
  using namespace client_messages;
  return std::visit([] <typename T> (T& x) -> ClientMessage {
      if constexpr(std::same_as<T, PlaceBomb> || std::same_as<T, PlaceBlock> || std::same_as<T, Move>)
        return x;
      else
        static_assert(always_false_v<T>, "Non-exhaustive pattern matching!");
    }, msg);
}

void RoboticClient::input_handler()
{
  using namespace client_messages;

  ClientMessage msg;
  InputMessage inp;

  for (;;) {
    dbg("[input_handler] waiting for input");
    gui_deser.readable().sock_fill(gui_socket, gui_endpoint);

    try {
      gui_deser >> inp;
      gui_deser.no_trailing_bytes();
    } catch (UnmarshallingError& e) {
      dbg("[input_handler] invalid input (ignored): ", e.what());
      continue;
    }

    if (game_state.lobby) {
      dbg("[input_handler] first input in the lobby --> trying to join");
      game_state.lobby = false;
      msg = Join{name};
    } else {
      msg = input_to_client(inp);
    }

    server_ser << msg;
    dbg("[input_handler] sending ", server_ser.size(), " bytes of input to the server");
    server_socket.send(boost::asio::buffer(server_ser.drain_bytes()));
  }
}

void RoboticClient::game_handler()
{
  ServerMessage updt;
  DisplayMessage msg;

  for (;;) {
    dbg("[game_handler] tying to read a message from server");
    server_deser >> updt;
    dbg("[game_handler] message read, proceeding to handle it!");
    server_msg_handler(updt);

    update_game();
    gui_ser << game_state.state;
    dbg("[game_handler] sending ",  gui_ser.size(), " bytes to gui");
    gui_socket.send_to(boost::asio::buffer(gui_ser.drain_bytes()), gui_endpoint);
  }
}

void RoboticClient::play()
{
  std::jthread input_worker{[this] () { input_handler(); }};
  std::jthread game_worker{[this] () { game_handler(); }};
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

    std::cout << "\t\tBOMBERPERSON\n\n";

    if (vm.count("help")) {
      std::cout << "Usage: " << argv[0] <<  " [flags]\n";
      std::cout << desc;
      return 0;
    }

    // notify about missing options only after printing help
    po::notify(vm);

    std::cout << "Selected options:\n"
         << "\tgui-address: " << gui_addr << "\n"
         << "\tplayer-name: " << player_name << "\n"
         << "\tserver-address: " << server_addr << "\n"
         << "\tport: " << portnum << "\n"
         << "Running the client with these.\n\n";

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

