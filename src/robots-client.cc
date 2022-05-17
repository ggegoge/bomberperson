
#include "netio.h"
#include "serialise.h"
#include "messages.h"

#include "dbg.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/resolver_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/registered_buffer.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <map>
#include <mutex>
#include <thread>
#include <type_traits>
#include <functional>
#include <regex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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

class ClientError : public std::runtime_error {
public:
  ClientError()
    : std::runtime_error("Client error!") {}

  ClientError(const std::string& msg) : std::runtime_error(msg) {}
};


namespace {

// Helper for visiting, inspired by cppref.
template<typename> inline constexpr bool always_false_v = false;

struct GameState {
  DisplayMessage state;
  map<BombId, server_messages::Bomb> bombs;
  // this bool will tell us whether to ignore the gui input or not.
  bool observer = false;
  bool game_on = false;
  uint16_t timer;
};

pair<string, string> get_addr(const string& addr)
{
  cerr << "addr=" << addr;
  static regex r("^(.*):(\\d+)$");
  smatch sm;

  if (regex_search(addr, sm, r)) {
    cerr << "matchess\n";
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
  mutex udp_mutex;
public:
  RoboticClient(const string& name, uint16_t portnum,
                const string& server_addr, const string& gui_addr)
    : name(name), server_socket(io_ctx), gui_socket(io_ctx, udp::endpoint(udp::v6(), portnum)),
      gui(), server(), server_deser(server_socket), gui_deser({})
  {
    auto [gui_ip, gui_port] = get_addr(gui_addr);
    cout << "gui_ip=" << gui_ip << ", " << "gui_port=" << gui_port << "\n";
    udp::resolver udp_resolver(io_ctx);
    gui = *udp_resolver.resolve(gui_ip, gui_port, resolver_base::numeric_service);

    auto [serv_ip, serv_port] = get_addr(server_addr);
    cout << "serv_ip=" << serv_ip << ", " << "serv_port=" << serv_port << "\n";
    tcp::resolver tcp_resolver(io_ctx);
    server = *tcp_resolver.resolve(serv_ip, serv_port, resolver_base::numeric_service);

    // todo: move to separate functions, innit?
    server_socket.connect(server);
    gui_socket.connect(gui);
  }

  // test whether we can say hello through both of out sockets
  void test()
  {
    using namespace display_messages;
    cout << "display via sockkk\n";
    DisplayMessage msg;
    struct Lobby l;
    l.server_name = "GNIOX";
    l.players_count = 11;
    l.size_x = 1234;
    l.size_y = 1;
    l.game_length = 0;
    l.explosion_radius = 11;
    l.bomb_timer = 17;
    struct server_messages::Player p1;
    p1.name = "KOT";
    p1.address = "1.2.3.4:0001";
    struct server_messages::Player p2;
    p2.name = name;
    p2.address = "4.3.2.1:2137";
    struct server_messages::Player p3;
    p3.name = "pojście spać!!!";
    p3.address = "5.7.3.8:0001";
    l.players.insert({1, p1});
    l.players.insert({2, p2});
    l.players.insert({3, p3});

    msg = l;
    server_ser.clean();
    server_ser << msg;
    vector<uint8_t> bytes = server_ser.drain_bytes();
    gui_socket.send(boost::asio::buffer(bytes));

    using namespace client_messages;
    // wyślemy Join("siemkaaa") -> [0, 8, s, i, ..., a]
    struct Join j("siemkaaa");
    ClientMessage send_msg = j;
    server_ser << send_msg;
    bytes = server_ser.drain_bytes();
    server_socket.send(boost::asio::buffer(bytes));
  }

  // This function will be executed by one of the threads to receive input form
  // the gui and send it forward to the server.
  void gui_to_server();

  // This function reads messages from the server and updates the game state by
  // aggregating all of the information coming from the server. Then after each
  // such update it should tell the gui to show what is going on appropriately.
  void server_to_gui();

private:
  // Utilities?

  // Variant to variant conversion.
  ClientMessage input_to_client(InputMessage& msg);

  // Game'ise the lobby, changes the held game state appropriately.
  void lobby_to_game();

  void fill_bombs();

  // Events affect the game
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

  // Handling of messages from the gui.
  void input_msg_handler(input_messages::InputMessage& msg);  
  void pbm_handler(struct client_messages::PlaceBomb& pbm);
  void pbl_handler(struct client_messages::PlaceBlock& pbl);
  void mv_handler(struct client_messages::Move& mv);

  // Have these two functions separately as they use the same socket?
  InputMessage recv_gui();
  void send_gui();
};

// TODO
// Can we assume we get hello only when game is already on?...
void RoboticClient::hello_handler(struct server_messages::Hello& h)
{
  using namespace display_messages;
  // Ignoring Hello when the game is already on.
  if (game_state.game_on)
    return;

  struct Lobby l{h.server_name, h.players_count, h.size_x, h.size_y, h.game_length,
    h.explosion_radius, h.bomb_timer, map<PlayerId, server_messages::Player>{}};

  game_state.timer = h.bomb_timer;
  game_state.state = l;
}

void RoboticClient::ap_handler(struct server_messages::AcceptedPlayer& ap)
{
  if (game_state.game_on)
    return;

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
  using namespace display_messages;
  if (game_state.game_on)
    return;

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
void RoboticClient::ge_handler(struct server_messages::GameEnded&) {}

void RoboticClient::pbm_handler(struct client_messages::PlaceBomb&) {}
void RoboticClient::pbl_handler(struct client_messages::PlaceBlock&) {}
void RoboticClient::mv_handler(struct client_messages::Move&) {}

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

InputMessage RoboticClient::recv_gui()
{
  lock_guard<mutex> lock{udp_mutex};
  InputMessage msg;
  gui_deser.readable().recv_from_sock(gui_socket);
  gui_deser >> msg;
  return msg;
}

void RoboticClient::send_gui()
{
  lock_guard<mutex> lock{udp_mutex};
  fill_bombs();
  server_ser << game_state.state;
  gui_socket.send(boost::asio::buffer(server_ser.drain_bytes()));
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

void RoboticClient::input_msg_handler(input_messages::InputMessage& msg)
{
  using namespace client_messages;
  visit([this] <typename T> (T& x) {
      if constexpr(same_as<T, struct PlaceBomb>)
        pbm_handler(x);
      else if constexpr(same_as<T, struct PlaceBlock>)
        pbl_handler(x);
      else if constexpr(same_as<T, struct Move>)
        mv_handler(x);
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
void RoboticClient::gui_to_server()
{
  // We start engaging with the server only after receiving (valid) input
  // and if we are not observers. Is such a loop good here? I think so...
  using namespace client_messages;
  ClientMessage msg;
  
  for (;;) {
    try {
      gui_deser.readable().recv_from_sock(gui_socket);
      if (!game_state.observer)
        break;
    } catch (exception& ignored) {}
  }

  struct Join j(name);
  msg = j;
  server_ser << msg;
  server_socket.send(boost::asio::buffer(server_ser.drain_bytes()));

  do {
    InputMessage inp;
    gui_deser.readable().recv_from_sock(gui_socket);
    gui_deser >> inp;
    msg = input_to_client(inp);
    server_ser << msg;
    server_socket.send(boost::asio::buffer(server_ser.drain_bytes()));
  } while (game_state.game_on);
}

void RoboticClient::server_to_gui()
{
  ServerMessage updt;
  DisplayMessage msg;
  for (;;) {
    server_deser >> updt;
    server_msg_handler(updt);
    gui_ser << game_state.state;
    gui_socket.send(boost::asio::buffer(gui_ser.drain_bytes()));
  }
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
         << "\tport=" << portnum << "\n";

    RoboticClient client(player_name, portnum, server_addr, gui_addr);
    client.test();

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

