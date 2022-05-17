
#include "readers.h"
#include "serialise.h"
#include "messages.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/resolver_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <map>
#include <thread>
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

namespace po = boost::program_options;

using boost::asio::ip::udp;
using boost::asio::ip::tcp;

using boost::asio::ip::resolver_base;

using namespace std;

using input_messages::InputMessage;
using display_messages::DisplayMessage;
using server_messages::ServerMessage;
using client_messages::ClientMessage;

// todo: add trying and catching of that wherever it is advisable!
class ClientError : public std::runtime_error {
public:
  ClientError()
    : std::runtime_error("Client error!") {}

  ClientError(const std::string& msg) : std::runtime_error(msg) {}
};


namespace {

// Helper for visiting mimicking pattern matching, inspired by cppref.
template<typename> inline constexpr bool always_false_v = false;

struct GameState {
  DisplayMessage state;
  map<BombId, server_messages::Bomb> bombs;
  // this bool will tell us whether to ignore the gui input or not.
  bool observer = false;
  bool lobby = true;
  // data from the server
  uint16_t timer;
  uint8_t players_count;
  uint16_t explosion_radius;
};

// todo: address of form ::1:40007 is parsed but [::1]:40007 should be too!
pair<string, string> get_addr(const string& addr)
{
  static regex r("^(.*):(\\d+)$");
  smatch sm;

  if (regex_search(addr, sm, r)) {
    return {sm[1].str(), sm[2].str()};
  } else {
    throw ClientError("Invalid address!");
  }
}

};

class RoboticClient {
  boost::asio::io_context io_ctx;
  string name;
  tcp::socket server_socket;
  udp::socket gui_socket;
  udp::endpoint gui;
  tcp::endpoint server;
  Serialiser server_ser;
  Serialiser gui_ser;
  Deserialiser<ReaderTCP> server_deser;
  Deserialiser<ReaderUDP> gui_deser;
  GameState game_state;
public:
  RoboticClient(const string& name, uint16_t port,
                const string& server_addr, const string& gui_addr)
    : name(name), server_socket(io_ctx), gui_socket(io_ctx, udp::endpoint(udp::v6(), port)),
      gui(), server(), server_deser(server_socket), gui_deser({})
  {
    auto [gui_ip, gui_port] = get_addr(gui_addr);
    udp::resolver udp_resolver(io_ctx);
    gui = *udp_resolver.resolve(gui_ip, gui_port, resolver_base::numeric_service);

    if (gui.protocol() == udp::v6())
      cerr << "GUI: [" << gui.address() << "]:" << gui.port() << "\n";
    else
      cerr << "GUI: " << gui.address() << ":" << gui.port() << "\n";

    auto [serv_ip, serv_port] = get_addr(server_addr);
    tcp::resolver tcp_resolver(io_ctx);
    server = *tcp_resolver.resolve(serv_ip, serv_port, resolver_base::numeric_service);

    if (server.protocol() == tcp::v6())
      cerr << "SERVER: [" << server.address() << "]:" << server.port() << "\n";
    else
      cerr << "SERVER: " << server.address() << ":" << server.port() << "\n";

    // open connection to the server
    server_socket.connect(server);
    tcp::no_delay option(true);
    server_socket.set_option(option);

    // todo: gui needs to be turned on beforehand! should I really connect?
    // and to the gui...? Perhaps I should stick to send_to and receive_from?
    gui_socket.connect(gui);
  }

  // Main function for actually playing the game.
  void play();
  
private:
  // This function will be executed by one of the threads to receive input form
  // the gui and send it forward to the server.
  void input_handler();

  // This function reads messages from the server and updates the game state by
  // aggregating all of the information coming from the server. Then after each
  // such update it should tell the gui to show what is going on appropriately.
  void game_handler();

  // Variant to variant conversion.
  ClientMessage input_to_client(InputMessage& msg);

  // Game'ise the lobby, changes the held game state appropriately.
  void lobby_to_game();

  // Fill bombs in current game_state.state based on game_state.bombs.
  void fill_bombs();

  // Events affect the game.
  void apply_event(struct display_messages::Game& game,
                   map<BombId, server_messages::Bomb>& bombs,
                   const server_messages::EventVar& event);

  // Handling of messages from the server.
  void server_msg_handler(server_messages::ServerMessage& msg);
  void hello_handler(struct server_messages::Hello& hello);
  void ap_handler(struct server_messages::AcceptedPlayer& ap);
  void gs_handler(struct server_messages::GameStarted& gs);
  void turn_handler(struct server_messages::Turn& turn);
  void ge_handler(struct server_messages::GameEnded& ge);
};

// todo: receiving some messages like AccP or Hello _during the game_ is UB...
void RoboticClient::hello_handler(struct server_messages::Hello& h)
{
  cerr << "[game_handler] hello handler\n";

  using namespace display_messages;
  struct Lobby l{h.server_name, h.players_count, h.size_x, h.size_y, h.game_length,
    h.explosion_radius, h.bomb_timer, map<PlayerId, server_messages::Player>{}};

  game_state.timer = h.bomb_timer;
  game_state.state = l;
  game_state.players_count = h.players_count;
}

void RoboticClient::ap_handler(struct server_messages::AcceptedPlayer& ap)
{
  cerr << "[game_handler] ap_handler\n";

  visit([&ap] <typename GorL> (GorL& gl) {
      gl.players.insert({ap.id, ap.player});
    }, game_state.state);
}

void RoboticClient::lobby_to_game()
{
  using namespace display_messages;
  game_state.state = visit([] <typename GorL> (GorL& gl) {
      if constexpr(same_as<struct Lobby, GorL>) {
        map<PlayerId, Score> scores;
        for (auto& [plid, _] : gl.players)
          scores.insert({plid, 0});

        struct Game g{gl.server_name, gl.size_x, gl.size_y, gl.game_length,
          0, gl.players, {}, {}, {}, {}, scores};
        return DisplayMessage{g};
      } else if constexpr(same_as<struct Game, GorL>) {
        return DisplayMessage{gl};
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);
}

// do we get send this message only when we joined a game that 
void RoboticClient::gs_handler(struct server_messages::GameStarted& gs)
{
  cerr << "[game_handler] gs_handler\n";

  using namespace display_messages;
  game_state.observer = true;

  visit([&gs] <typename GorL> (GorL& gl) {
      gl.players = gs.players;
    }, game_state.state);

  lobby_to_game();
}

void RoboticClient::apply_event(struct display_messages::Game& game,
                                map<BombId, server_messages::Bomb>& bombs,
                                const server_messages::EventVar& event)
{
  cerr << "[game_handler] apply event\n";

  using namespace server_messages;
  visit([&game, &bombs, this] <typename Ev> (const Ev & ev) {
      if constexpr(same_as<struct BombPlaced, Ev>) {
        bombs.insert({ev.id, {ev.position, game_state.timer}});
      } else if constexpr(same_as<struct BombExploded, Ev>) {
        bombs.erase(ev.id);

        for (PlayerId plid : ev.killed)
          ++game.scores[plid];

        for (Position pos : ev.blocks_destroyed)
          game.blocks.erase(pos);
      } else if constexpr(same_as<struct PlayerMoved, Ev>) {
        game.player_positions[ev.id] = ev.position;
      } else if constexpr(same_as<struct BlockPlaced, Ev>) {
        game.blocks.insert(ev.position);
      } else {
        static_assert(always_false_v<Ev>, "Non-exhaustive pattern matching!");
      }
    }, event);
}

void RoboticClient::turn_handler(struct server_messages::Turn& turn)
{
  cerr << "[game_handler] turn handler, turn=" << turn.turn << "\n";

  lobby_to_game();
  struct display_messages::Game& current_game =
    get<struct display_messages::Game>(game_state.state);
  current_game.turn = turn.turn;

  for (const server_messages::EventVar& ev : turn.events) {
    apply_event(current_game, game_state.bombs, ev);
  }

  // upon each turn the bombs get their timers reduced.
  for (auto& [_, bomb] : game_state.bombs)
    --bomb.timer;
}

// TODO: what to do with the scores? we go into lobby state do we not
void RoboticClient::ge_handler(struct server_messages::GameEnded& ge)
{
  cerr << "[game_handler] ge_handler\n";

  using namespace display_messages;
  // note: we do not care about races towards "lobby"
  game_state.lobby = true;
  game_state.bombs = {};

  const map<PlayerId, server_messages::Player>& players =
    visit([] <typename GorL> (const GorL& gl) {
      if constexpr(same_as<struct Lobby, GorL> || same_as<struct Game, GorL>) {
        return gl.players;
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);

  cout << "GAME ENDED!!!\n";

  for (auto [id, score] : ge.scores) {
    cout << id << " " << players.at(id).name << "@" << players.at(id).address
         << " got killed " << score << " times!\n";
  }

  game_state.state = visit([this] <typename GorL> (GorL& gl) {
      if constexpr(same_as<struct Lobby, GorL>) {
        return DisplayMessage{gl};
      } else if constexpr(same_as<struct Game, GorL>) {
        struct Lobby l{gl.server_name, game_state.players_count, gl.size_x,
          gl.size_y, gl.game_length, game_state.explosion_radius, game_state.timer, {}};
        return DisplayMessage{l};
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);
}

void RoboticClient::fill_bombs()
{
  using namespace display_messages;

  visit([this] <typename GorL> (GorL& gl) {
      if constexpr(same_as<struct Lobby, GorL>) {
        // No bombs in the lobby.
        return;
      } else if constexpr(same_as<struct Game, GorL>) {
        gl.bombs = {};

        for (auto& [_, bomb] : game_state.bombs) {
          // todo: insertion does not do anything if an object is already in
          gl.bombs.insert(bomb);
        }
      } else {
        static_assert(always_false_v<GorL>, "Non-exhaustive pattern matching!");
      }
    }, game_state.state);
}

// Implementation of the server message handlers. Sadly there's no pattern
// matching in C++ (yet, we were born too early) so std::visit must do.
void RoboticClient::server_msg_handler(server_messages::ServerMessage& msg)
{
  using namespace server_messages;
  visit([this] <typename T> (T& x) {
      if constexpr(same_as<T, struct Hello>)
        hello_handler(x);
      else if constexpr(same_as<T, struct AcceptedPlayer>)
        ap_handler(x);
      else if constexpr(same_as<T, struct GameStarted>)
        gs_handler(x);
      else if constexpr(same_as<T, struct Turn>)
        turn_handler(x);
      else if constexpr(same_as<T, struct GameEnded>)
        ge_handler(x);
      else
        static_assert(always_false_v<T>, "Non-exhaustive pattern matching!");
    }, msg);
}

ClientMessage RoboticClient::input_to_client(input_messages::InputMessage& msg)
{
  using namespace client_messages;
  return visit([] <typename T> (T& x) {
      if constexpr(same_as<T, struct PlaceBomb> || same_as<T, struct PlaceBlock> ||
                   same_as<T, struct Move>)
        return ClientMessage{x};
      else
        static_assert(always_false_v<T>, "Non-exhaustive pattern matching!");
    }, msg);
}


// upon receiving first input from the gui we should send Join(name) to the
// server unless the game is already on and we only observe it.
void RoboticClient::input_handler()
{
  // We start engaging with the server only after receiving (valid) input
  // and if we are not observers. Is such a loop good here? I think so...
  using namespace client_messages;

  ClientMessage msg;
  InputMessage inp;

  for (;;) {
    cerr << "[input_handler] waiting for input\n";
    // todo: add methods for recv and send etc that wrap around that?
    gui_deser.readable().recv_from_sock(gui_socket);
    // todo: ignore invalid messages!
    gui_deser >> inp;

    if (game_state.lobby) {
      cerr << "[input_handler] first input in the lobby --> trying to join\n";
      game_state.lobby = false;
      struct Join j(name);
      msg = j;
    } else {
      msg = input_to_client(inp);
    }

    server_ser << msg;
    cerr << "[input_handler] sending " << server_ser.size()
         << " bytes of input to the server\n";
    server_socket.send(boost::asio::buffer(server_ser.drain_bytes()));
  }
}

void RoboticClient::game_handler()
{
  ServerMessage updt;
  DisplayMessage msg;

  for (;;) {
    cerr << "[game_handler] tying to read a message from server\n";
    // todo: disconnect upon receiving a bad message. Should try to reconnect?
    server_deser >> updt;
    cerr << "[game_handler] message read, proceeding to handle it!\n";
    server_msg_handler(updt);

    fill_bombs();
    gui_ser << game_state.state;
    cerr << "[game_handler] sending " << gui_ser.size() << " bytes to gui\n";
    gui_socket.send(boost::asio::buffer(gui_ser.drain_bytes()));
  }
}

// we play while we can innit
void RoboticClient::play()
{
  jthread input_worker([this] () {
    input_handler();
  });

  jthread game_worker([this] () {
    game_handler();
  });
}

// this will be used for sure.
int main(int argc, char* argv[])
{
  try {
    uint16_t portnum;
    string gui_addr;
    string player_name;
    string server_addr;
    po::options_description desc("Allowed flags for the robotic client");
    desc.add_options()
      ("help,h", "produce this help message")
      ("gui-address,d", po::value<string>(&gui_addr)->required(),
       "gui address, (IPv4):(port) or (IPv6):(port) or (hostname):(port)")
      ("player-name,n", po::value<string>(&player_name)->required(), "player name")
      ("server-address,s", po::value<string>(&server_addr)->required(),
       "server address, same format as gui address")
      ("port,p", po::value<uint16_t>(&portnum)->required(),
       "listen to gui on a port.")
    ;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
              options(desc).run(), vm);

    if (vm.count("help")) {
      cout << "Usage: " << argv[0] <<  " [flags]\n";
      cout << desc;
      return 0;
    }

    // notify about missing options only after printing help
    po::notify(vm);

    cout << "Selected options:\n"
         << "\tgui-address=" << gui_addr << "\n"
         << "\tplayer-name=" << player_name << "\n"
         << "\tserver-address=" << server_addr << "\n"
         << "\tport=" << portnum << "\n"
         << "Running the client with those.\n";

    RoboticClient client(player_name, portnum, server_addr, gui_addr);
    client.play();

  } catch (po::required_option& e) {
    cerr << "Missing some options: " << e.what() << "\n";
    cerr << "See " << argv[0] << " -h for help.\n";
    return 1;
  } catch (ClientError& e) {
    cerr << "Client error: " << e.what() << "\n";
  } catch (std::exception& e) {
    cerr << "Other exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}

