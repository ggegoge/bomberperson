// The server for the bomberperson game.

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/resolver_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <condition_variable>
#include <chrono>
#include <random>
#include <map>
#include <mutex>
#include <optional>
#include <semaphore>
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

namespace po = boost::program_options;

using boost::asio::ip::tcp;
using boost::asio::ip::resolver_base;

using std::chrono::system_clock;

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

constexpr size_t MAX_CLIENTS = 25;

// Helper for std::visiting mimicking pattern matching, inspired by cppref.
template<typename> inline constexpr bool always_false_v = false;

class ServerError : public std::runtime_error {
public:
  ServerError()
    : runtime_error("Server error!") {}

  ServerError(const std::string& msg) : std::runtime_error{msg} {}
};

class ServerLogicError : public std::logic_error {
public:
  ServerLogicError()
    : logic_error("Fatal error in server's logic.") {}

  ServerLogicError(const std::string& msg) : std::logic_error{msg} {}
};


// https://stackoverflow.com/a/12805690/9058764
template <typename T>
class BlockingQueue
{
private:
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<T> queue;
public:
  void push(T const& value)
  {
    {
      std::lock_guard<std::mutex> lock{mutex};
      queue.push_front(value);
    }
    cv.notify_one();
  }

  T pop()
  {
    std::unique_lock<std::mutex> lock{mutex};
    cv.wait(lock, [this] { return !queue.empty(); });
    T elem{std::move(queue.back())};
    queue.pop_back();
    return elem;
  }
};

// todo: sep struct?
struct ServerParameters {};

struct ConnectedClient {
  tcp::socket sock;
  bool in_game = false;
  std::optional<ClientMessage> current_move;
  uint8_t id;
};

std::string address_from_sock(const tcp::socket& sock)
{
  std::string ip = sock.remote_endpoint().address().to_string();
  if (sock.remote_endpoint().protocol() == tcp::v6())
    ip = "[" + ip + "]";

  std::string port = boost::lexical_cast<std::string>(sock.remote_endpoint().port());

  return ip + ":" + port;
}

template <std::integral K, typename V>
K get_free_id(const std::map<K, V>& m)
{
  if (m.empty()) {
    return 0;
  } else {
    const auto& [id, _] = *--m.end();
    return id + 1;
  }
}

class RoboticServer {
  // basic server parameters
  std::string name;
  uint16_t timer;
  uint8_t players_count;
  uint64_t turn_duration;
  uint16_t radius;
  uint16_t initial_blocks;
  uint16_t game_length;
  uint32_t seed;
  uint16_t size_x;
  uint16_t size_y;

  uint16_t port;

  // networking
  boost::asio::io_context io_ctx;
  tcp::endpoint endpoint;
  tcp::acceptor tcp_acceptor;

  
  // Game handling data:
  std::vector<std::optional<ConnectedClient>> clients =
    std::vector<std::optional<ConnectedClient>>(MAX_CLIENTS);
  // ^ you have to declare this way, im not kidding
  // https://stackoverflow.com/a/49637243/9058764
  std::vector<std::mutex> clients_mutices =
    std::vector<std::mutex>(MAX_CLIENTS);
  // todo: this variable should be atomic i think
  size_t number_of_clients = 0;
  BlockingQueue<std::pair<size_t, server_messages::Player>> joined;

  server_messages::Hello hello;
  server_messages::Turn turn0{0, {}};

  // threading
  std::counting_semaphore<MAX_CLIENTS> clients_sem{MAX_CLIENTS};
  std::mutex acceptor_mutex;
  std::mutex game_master_mutex;
  // for hailer and joiner
  std::mutex players_mutex;
  std::mutex turn0_mutex;
  // waiting for players to join
  std::condition_variable for_game;
  // waiting for slots to accept
  std::condition_variable for_places;

  // rand
  std::minstd_rand rand;

  // Current game state:

  // this indicates whether we are currently in lobby state or not
  bool lobby = true;
  std::map<PlayerId, server_messages::Player> players;
  std::set<PlayerId> killed_this_turn;
  std::map<PlayerId, size_t> playing_clients;
  std::map<PlayerId, Position> positions;
  std::map<BombId, server_messages::Bomb> bombs;
  std::map<PlayerId, Score> scores;
  std::set<Position> blocks;
  std::vector<Position> explosions;
public:
  RoboticServer(const std::string& name, uint16_t timer, uint8_t players_count,
                uint64_t turn_duration, uint16_t radius, uint16_t initial_blocks,
                uint16_t game_length, uint32_t seed, uint16_t size_x, uint16_t size_y,
                uint16_t port)
    : name{name}, timer{timer}, players_count{players_count}, turn_duration{turn_duration},
      radius{radius}, initial_blocks{initial_blocks}, game_length{game_length},
      seed{seed}, size_x{size_x}, size_y{size_y}, port{port}, io_ctx{},
      endpoint(tcp::v6(), port), tcp_acceptor{io_ctx, endpoint},
      hello{name, players_count, size_x, size_y, game_length, radius, timer}, rand{seed} {}

  void run();

private:
  // Functions to be executed by different kinds of threads.

  // This thread handles incoming connections, accepts them (if there is enough
  // place on the server), hails them and then assigns a reading thread to them.
  void acceptor();

  // This thread works in a loop and after each turn gathers input from playing
  // clients and then applies their moves when it is possible. Having done that
  // it adds the events to turn0 and makes a current turn object to be sent to all
  // connected clients.
  void game_master();

  // This thread decides who is suitable to join the game. It works during lobby
  // state and having positevly reviewed a join request it notifies this client's
  // handler about that, assigns them an id and sends AcceptedPlayer message to
  // all connected clients.
  void join_handler();

  // This is the basic thread that handles input from a single client and manages
  // one ConnectedClient in the vector of all connected clients.
  void client_handler(size_t client_index);

  // Helper and utility functions of all kinds

  // This sends the welcome info to the newly connected client.
  void hail(tcp::socket& client);

  // starting and ending a game
  server_messages::Turn start_game();
  void end_game();

  // this function does all bombing related events
  void do_bombing(server_messages::Turn& turn);

  // gather moves
  void gather_moves(server_messages::Turn& turn);

  Position do_move(Position pos, client_messages::Direction dir) const;

  void send_to_all(const ServerMessage& msg);
};

// Utility functions
void RoboticServer::hail(tcp::socket& client)
{
  Serialiser ser;
  ser << ServerMessage{hello};
  client.send(boost::asio::buffer(ser.drain_bytes()));

  {
    std::lock_guard<std::mutex> lk{players_mutex};
    if (lobby) {
      server_messages::GameStarted gs{players};
      ser << ServerMessage{gs};
      client.send(boost::asio::buffer(ser.drain_bytes()));
      std::lock_guard<std::mutex> lk{turn0_mutex};
      ser << ServerMessage{turn0};
      client.send(boost::asio::buffer(ser.drain_bytes()));
    } else {
      for (const auto& [plid, player] : players) {
        server_messages::AcceptedPlayer ap{plid, player};
        ser << ServerMessage{ap};
        client.send(boost::asio::buffer(ser.drain_bytes()));
      }
    }
  }

}

void RoboticServer::send_to_all(const ServerMessage& msg)
{
  Serialiser ser;
  ser << msg;

  for (size_t i = 0; i < clients.size(); ++i) {
    std::optional<ConnectedClient>& cm = clients.at(i);
    std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
    if (cm.has_value()) {
      try {
        cm->sock.send(boost::asio::buffer(ser.to_bytes()));
      } catch (std::exception& e) {
        // ignorint the exception, will get handled by the client thread
        dbg("[send_to_all] failed to send to client ", i);
        continue;
      }
    }
  }
}

// todo do bombing
void RoboticServer::do_bombing(server_messages::Turn&)
{
}

void RoboticServer::gather_moves(server_messages::Turn& turn)
{
  for (const auto& [id, idx] : playing_clients) {
    std::lock_guard<std::mutex> lk{clients_mutices.at(idx)};
    std::optional<ConnectedClient>& maybe_cl = clients.at(idx);

    if (!maybe_cl.has_value()) {
      dbg("[gather_moves] client ", idx, " ie player ", (int)id, " disconnected");
      continue;
    }

    if (!maybe_cl->in_game) {
      dbg("[gather_moves] client at ", idx, " no longer in game");
      continue;
    }

    if (!maybe_cl->current_move.has_value()) {
      dbg("[gather_moves] client ", idx, " ie player ", (int)id, " has not moved");
      continue;
    }

    if (!killed_this_turn.contains(id)) {
      using namespace client_messages;
      PlayerId plid = id;
      const ClientMessage& cmsg = maybe_cl->current_move.value();
      std::visit([this, &turn, plid] <typename Cm> (const Cm& cm) {
          if constexpr(std::same_as<Cm, Join>) {
            throw ServerLogicError("Join should not be placed as current move!");
          } else if constexpr(std::same_as<Cm, PlaceBomb>) {
            // get an id for the bomb
            BombId bombid = get_free_id(bombs);
            server_messages::Bomb bomb{positions.at(plid), timer};
            bombs.insert({bombid, bomb});
            server_messages::BombPlaced bp{bombid, bomb.first};
            turn.second.push_back(bp);
          } else if constexpr(std::same_as<Cm, PlaceBlock>) {
            blocks.insert(positions.at(plid));
          } else if constexpr(std::same_as<Cm, Move>) {
            Position pos = positions.at(plid);
            Position new_pos = do_move(pos, cm);
            if (pos != new_pos) {
              positions.at(plid) = new_pos;
              server_messages::PlayerMoved pm{plid, new_pos};
              turn.second.push_back(pm);
            }
          } else {
            static_assert(always_false_v<Cm>, "Non-exhaustive pattern matching!");
          }
        }, cmsg);
    } else {
      Position new_pos = {rand() % size_x, rand() % size_y};
      positions[id] = new_pos;
      server_messages::PlayerMoved pm{id, new_pos};
      turn.second.push_back(pm);
    }
  }
}

Position RoboticServer::do_move(Position pos,
                                client_messages::Direction dir) const
{
  using namespace client_messages;
  auto [x, y] = pos;

  switch (dir) {
  case Direction::UP:
    return (y + 1 < size_y) ? Position{x, y + 1} : pos;
  case Direction::DOWN:
    return (y > 0) ? Position{x, y - 1} : pos;

  case Direction::LEFT:
    return (x > 0) ? Position{x - 1, y} : pos;

  case Direction::RIGHT:
    return (x + 1 < size_x) ? Position{x + 1, y} : pos;

  case Direction::BOLLOCKS:
  default:
    return pos;
  }
}

// todo: make the zeroth turn
server_messages::Turn RoboticServer::start_game()
{
  players = {};
  killed_this_turn = {};
  positions = {};
  bombs = {};
  scores = {};
  blocks = {};
  explosions = {};
  // todo: generate random board
  return {0, {}};
}

void RoboticServer::end_game()
{
  send_to_all(ServerMessage{scores});

  for (size_t i = 0; i < clients.size(); ++i) {
    std::optional<ConnectedClient>& cm = clients.at(i);
    std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
    if (cm.has_value()) {
      cm->in_game = false;
    }
  }
}

// Thread functions
void RoboticServer::acceptor()
{
  for (;;) {
    if (number_of_clients > MAX_CLIENTS)
      throw ServerLogicError{
        "Number of connected clients shouldn't exceed the max number of clients!"};

    if (number_of_clients == MAX_CLIENTS) {
      std::unique_lock lk{acceptor_mutex};
      for_places.wait(lk, [this] {return number_of_clients < MAX_CLIENTS;});
    }

    tcp::socket new_client(io_ctx);
    tcp_acceptor.accept(new_client);
    try {
      hail(new_client);
    } catch (std::exception& e) {
      // disconnected during hailing? au revoir then mate!
      continue;
    }

    // having hailed the client find him a place in the array.
    // there should be a place as number_of_clients has been checked beforehand
    ConnectedClient cl{std::move(new_client), false, {}, 0};
    for (size_t i = 0; i < clients.size(); ++i) {
      if (clients.at(i).has_value())
        continue;

      ++number_of_clients;
      // do not need a mutex here as it is none
      clients.at(i) = std::move(cl);
      std::jthread th{[this, i] { client_handler(i); }};
      // detached as it may quit or unquit independently of anything?
      th.detach();
      break;
    }
  }
}

void RoboticServer::client_handler(size_t i)
{
  using namespace client_messages;
  Deserialiser<ReaderTCP> deser{clients.at(i)->sock};
  for (;;) {
    try {
      ClientMessage msg;
      deser >> msg;
      {
        std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
        std::visit([this, i] <typename Cm> (const Cm& cm) {
            if constexpr(std::same_as<Cm, Join>) {
              if (!clients.at(i)->in_game) {
                std::string addr = address_from_sock(clients.at(i)->sock);
                joined.push({i, {cm, addr}});
              }
            } else {
              clients.at(i)->current_move = cm;
            }
          }, msg);
      }
    } catch (std::exception& e) {
      // upon disconnection this thread says au revoir
      std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
      clients.at(i) = {};
      --number_of_clients;
      for_places.notify_all();
      return;
    }
  }
}

void RoboticServer::join_handler()
{
  for (;;) {
    // all players are here...
    if (players.size() == players_count) {
      lobby = false;
      // wake up the game master, he has waited enough did he not
      for_game.notify_all();
    }

    auto [i, player] = joined.pop();

    if (!lobby)
      continue;

    uint8_t id = get_free_id(players);
    bool accepted = false;
    {
      std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
      if (clients.at(i).has_value()) {
        clients.at(i)->id = id;
        clients.at(i)->in_game = true;
        players.insert({id, player});
        playing_clients.insert({id, i});
        accepted = true;
      }
    }

    if (accepted)
      send_to_all(ServerMessage{server_messages::AcceptedPlayer{id, player}});
  }
}

void RoboticServer::game_master()
{
  size_t turn_number = 0;
  for (;;) {
    server_messages::Turn current_turn{turn_number, {}};
    if (turn_number == game_length || lobby) {
      lobby = true;
      std::unique_lock lk{game_master_mutex};
      for_game.wait(lk, [this] { return !lobby; });
      // we woken up, the game has just started. Let's get this going then shall we.
      current_turn = turn0 = start_game();
    }

    // wait for the turn to pass...
    std::this_thread::sleep_for(std::chrono::milliseconds(turn_duration));

    // todo input from playing clients
    if (turn_number > 0) {
      killed_this_turn = {};
      do_bombing(current_turn);
      gather_moves(current_turn);
      std::lock_guard<std::mutex> lk{turn0_mutex};
      // append those events to turn0
      for (const server_messages::Event& ev : current_turn.second)
        turn0.second.push_back(ev);
    }

    // todo send it to clients
    send_to_all(ServerMessage{current_turn});

    ++turn_number;
    // todo game ended --> send ths information to everyone and then set
    // in game to false and at the end set lobby to true and wait in the next it
    if (turn_number == game_length) {
      end_game();
    }
  }
}

// Main server function.
void RoboticServer::run()
{
  std::jthread gm_th{[this] { game_master(); }};
  std::jthread jh_th{[this] { join_handler(); }};
  std::jthread acc_th{[this] { acceptor(); }};
}

} // namespace anonymous

int main(int argc, char* argv[])
{
  try {
    std::string name;
    uint16_t timer;
    uint8_t players_count;
    uint64_t turn_duration;
    uint16_t radius;
    uint16_t initial_blocks;
    uint16_t game_length;
    uint32_t seed;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t port;
    
    po::options_description desc{"Allowed flags for the robotic client"};
    desc.add_options()
      ("help,h", "produce this help message")
      ("server-name,n", po::value<std::string>(&name)->required(), "player name")
      ("port,p", po::value<uint16_t>(&port)->required(),
       "listen on port")
      ("bomb-timer,b", po::value<uint16_t>(&timer)->required())
      ("turn-duration,d", po::value<uint64_t>(&turn_duration)->required())
      ("players-count,c", po::value<uint8_t>(&players_count)->required())
      ("explosion-radius,e", po::value<uint16_t>(&radius)->required())
      ("initial-blocks,k", po::value<uint16_t>(&initial_blocks)->required())
      ("game-length,l", po::value<uint16_t>(&game_length)->required())
      ("seed,s", po::value<uint32_t>(&seed)->default_value(
        static_cast<uint32_t>(system_clock::now().time_since_epoch().count())),
        "randomness' seed, defult is current unix time")
      ("size-x,x", po::value<uint16_t>(&size_x)->required())
      ("size-y,y", po::value<uint16_t>(&size_y)->required())
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

    // notify about missing options only after printing help
    po::notify(vm);

    RoboticServer server{name, timer, players_count, turn_duration, radius, initial_blocks,
      game_length, seed, size_x, size_y, port};

    server.run();
  } catch (po::required_option& e) {
    std::cerr << "Missing some options: " << e.what() << "\n";
    std::cerr << "See " << argv[0] << " -h for help.\n";
    return 1;
  } catch (ServerError& e) {
    std::cerr << "Server error: " << e.what() << "\n";
  } catch (std::exception& e) {
    std::cerr << "Other exception: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
