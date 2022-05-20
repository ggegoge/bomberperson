// The server for the bomberperson game.

// todo: add spaces after if constexpr?
// todo: monitor all lock guards and check scopings reasonability
// todo: no deadlocks
// todo: main thread just waits on join, innit?
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <limits>
#include <random>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <atomic>
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
  if constexpr(debug) {
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
    : logic_error{"Fatal error in server's logic."} {}

  ServerLogicError(const std::string& msg) : std::logic_error{msg} {}
};


// https://stackoverflow.com/a/12805690/9058764
template <typename T>
class BlockingQueue {
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

// todo: sep struct?????
struct ServerParameters {};

// This structure holds relevant information for a single connected client.
struct ConnectedClient {
  tcp::socket sock;
  bool in_game = false;
  std::optional<ClientMessage> current_move;
  uint8_t id;
};

// Get clients address in textual form (ip:port) from tcp socket.
std::string address_from_sock(const tcp::socket& sock)
{
  std::string ip = sock.remote_endpoint().address().to_string();
  if (sock.remote_endpoint().protocol() == tcp::v6())
    ip = "[" + ip + "]";

  std::string port = boost::lexical_cast<std::string>(sock.remote_endpoint().port());

  return ip + ":" + port;
}

// Utility function for finding a free id in a map with integral keys.
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
  // Basic server parameters that should be known at all times.
  std::string name;
  const uint16_t timer;
  const uint8_t players_count;
  const uint64_t turn_duration;
  const uint16_t radius;
  const uint16_t initial_blocks;
  const uint16_t game_len;
  const uint16_t size_x;
  const uint16_t size_y;

  // Networking.
  boost::asio::io_context io_ctx;
  tcp::endpoint endpoint;
  tcp::acceptor tcp_acceptor;
  
  // Game handling data.
  // Vector of clients who are connected with us.
  std::vector<std::optional<ConnectedClient>> clients =
    std::vector<std::optional<ConnectedClient>>(MAX_CLIENTS);
  // ^ you have to declare this way, im not kidding
  // https://stackoverflow.com/a/49637243/9058764

  // Mutex to guard each connected client.
  std::vector<std::mutex> clients_mutices =
    std::vector<std::mutex>(MAX_CLIENTS);

  // Count of currently connected clients ie. non "none" slots in clients.
  std::atomic_size_t number_of_clients = 0;

  // Queue for all join requests.
  BlockingQueue<std::pair<size_t, server_messages::Player>> joined;

  // The "Hello" message sent by our server does not change throughout its work.
  const server_messages::Hello hello;

  // This turn is the zeroth turn that keeps all events since the game begun.
  server_messages::Turn turn0{0, {}};

  // For synchronisation.

  // For "acceptor" thread to wait for free spaces in clients vector for clients.
  std::mutex acceptor_mutex;
  std::condition_variable for_places;

  // For "game_master" thread to wait for required number of players to join.
  std::mutex game_master_mutex;
  std::condition_variable for_game;

  // Protection of variables.
  std::mutex players_mutex;
  std::mutex playing_clients_mutex;
  std::mutex turn0_mutex;

  // Random number generator used by the server.
  std::minstd_rand rand;

  // Current game state:
  std::map<PlayerId, server_messages::Player> players;
  std::set<PlayerId> killed_this_turn;
  std::map<PlayerId, size_t> playing_clients;
  std::map<PlayerId, Position> positions;
  std::map<BombId, server_messages::Bomb> bombs;
  std::map<PlayerId, Score> scores;
  std::set<Position> blocks;
  std::set<Position> destroyed_this_turn;
  std::vector<BombId> explosions;

  // This indicates whether we are currently in lobby state or not
  bool lobby = true;
public:
  RoboticServer(const std::string& name, uint16_t timer, uint8_t players_count,
                uint64_t turn_duration, uint16_t radius, uint16_t initial_blocks,
                uint16_t game_len, uint32_t seed, uint16_t size_x, uint16_t size_y,
                uint16_t port)
    : name{name}, timer{timer}, players_count{players_count}, turn_duration{turn_duration},
      radius{radius}, initial_blocks{initial_blocks}, game_len{game_len}, size_x{size_x},
      size_y{size_y}, io_ctx{}, endpoint(tcp::v6(), port), tcp_acceptor{io_ctx, endpoint},
      hello{name, players_count, size_x, size_y, game_len, radius, timer}, rand{seed}
  {
    std::cout << "Running the server \"" << name << "\" on "
              << endpoint.address() << endpoint.port() << "\n";
  }

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

  // Starting and ending a game. Starting is creating the initial turn0.
  server_messages::Turn start_game();
  void end_game();

  // This function does all bombing related events.
  void do_bombing(server_messages::Turn& turn);

  // funcs: find_neighbouring and find_in_radius(pos, direction) -> players
  void explode_in_radius(std::set<PlayerId>& killed, std::set<Position>& destroyed,
                        Position pos, client_messages::Direction dir);

  void kill_on_position(std::set<PlayerId>& killed, Position pos);

  // This gathers all moves from connected playing clients, processes them and
  // adds to the current turn's event list.
  void gather_moves(server_messages::Turn& turn);

  // Simulating a move in direction dir from position pos.
  Position do_move(Position pos, client_messages::Direction dir) const;

  // Utilities for sending.

  // Send a message to all connected clients.
  void send_to_all(const ServerMessage& msg);

  // Wrapper for sending to a specific client socket, the second does not fail.
  void send_bytes(const std::vector<uint8_t>& bytes, tcp::socket& sock);
  bool try_send_bytes(const std::vector<uint8_t>& bytes, tcp::socket& sock);
};

// Utility functions.
void RoboticServer::hail(tcp::socket& client)
{
  dbg("[acceptor] Haling new client.");
  const auto& [hname, hpc, hx, hy, hgl, hr, ht] = hello;
  dbg("[acceptor] sending Hello{\"", hname, "\"", ", ", static_cast<int>(hpc),
      ", ", hx, ", ", hy, ", ", hgl, ", ", hr, ", ", ht, "}.");
  Serialiser ser;
  ser << ServerMessage{hello};

  if (!lobby) {
    dbg("[acceptor] Client late innit, sending GameStarted.");
    server_messages::GameStarted gs;
    {
      std::lock_guard<std::mutex> lk{players_mutex};
      gs = players;
    }
    ser << ServerMessage{gs};
    {
      std::lock_guard<std::mutex> lk{turn0_mutex};
      ser << ServerMessage{turn0};
      dbg("[acceptor] Sending turn0 with ", turn0.second.size(), " events.");
    }
  } else {
    dbg("[acceptor] Sending players as a series of AcceptedPlayer messages.");
    std::lock_guard<std::mutex> lk{players_mutex};
    for (const auto& [plid, player] : players) {
      server_messages::AcceptedPlayer ap{plid, player};
      ser << ServerMessage{ap};
    }
  }
  dbg("[acceptor] Hailing with ", ser.size(), " bytes.");
  send_bytes(ser.drain_bytes(), client);
}

void RoboticServer::send_bytes(const std::vector<uint8_t>& bytes, tcp::socket& sock)
{
  sock.send(boost::asio::buffer(bytes));
}

bool RoboticServer::try_send_bytes(const std::vector<uint8_t>& bytes, tcp::socket& sock)
{
  try {
    send_bytes(bytes, sock);
    return true;
  } catch (std::exception& e) {
    return false;
  }
}

void RoboticServer::send_to_all(const ServerMessage& msg)
{
  Serialiser ser;
  ser << msg;

  for (size_t i = 0; i < clients.size(); ++i) {
    std::optional<ConnectedClient>& cm = clients.at(i);
    std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
    if (cm.has_value())
      try_send_bytes(ser.to_bytes(), cm->sock);
  }
}

void RoboticServer::do_bombing(server_messages::Turn& turn)
{
  auto& [_, events] = turn;

  for (auto& [bombid, bomb] : bombs) {
    auto& [bomb_pos, bomb_timer] = bomb;
    --bomb_timer;
    if (bomb_timer == 0) {
      explosions.push_back(bombid);
      std::set<PlayerId> killed;
      std::set<Position> destroyed;

      client_messages::Direction dirs[] = {client_messages::Up{},
        client_messages::Down{}, client_messages::Left{}, client_messages::Right{}};

      // Go in all directions and do the explosive bit of action.
      for (client_messages::Direction d : dirs)
        explode_in_radius(killed, destroyed, bomb_pos, d);

      events.push_back(server_messages::BombExploded{bombid, killed, destroyed});
    }
  }
}

void RoboticServer::gather_moves(server_messages::Turn& turn)
{
  std::lock_guard<std::mutex> lk{playing_clients_mutex};
  for (const auto& [id, idx] : playing_clients) {
    std::lock_guard<std::mutex> lk{clients_mutices.at(idx)};
    std::optional<ConnectedClient>& maybe_cl = clients.at(idx);

    if (!maybe_cl.has_value() || !maybe_cl->in_game)
      throw ServerLogicError{"Clients in playing_clients should be in game!"};

    if (!killed_this_turn.contains(id)) {
      if (!maybe_cl->current_move.has_value()) {
        dbg("[game_master] Client ", idx, " ie player ", static_cast<int>(id),
            " has not moved.");
        continue;
      }

      using namespace client_messages;
      PlayerId plid = id;
      const ClientMessage& cmsg = maybe_cl->current_move.value();

      // Pattern match the player's action.
      std::visit([this, &turn, plid] <typename Cm> (const Cm& cm) {
          auto& [_, events] = turn;
          if constexpr(std::same_as<Cm, Join>) {
            throw ServerLogicError("Join should not be placed as current move!");
          } else if constexpr(std::same_as<Cm, PlaceBomb>) {
            // Get an id for the new bomb.
            BombId bombid = get_free_id(bombs);
            server_messages::Bomb bomb{positions.at(plid), timer};
            bombs.insert({bombid, bomb});
            server_messages::BombPlaced bp{bombid, bomb.first};
            events.push_back(bp);
          } else if constexpr(std::same_as<Cm, PlaceBlock>) {
            Position pos = positions.at(plid);
            blocks.insert(pos);
            events.push_back(server_messages::BlockPlaced{pos});
          } else if constexpr(std::same_as<Cm, Move>) {
            Position pos = positions.at(plid);
            Position new_pos = do_move(pos, cm);
            if (!blocks.contains(new_pos) && pos != new_pos) {
              positions.at(plid) = new_pos;
              server_messages::PlayerMoved pm{plid, new_pos};
              events.push_back(pm);
            }
          } else {
            static_assert(always_false_v<Cm>, "Non-exhaustive pattern matching!");
          }
        }, cmsg);
    }

    // We do not want this move to stay here before the next turn.
    maybe_cl->current_move = {};
  }
}

void RoboticServer::kill_on_position(std::set<PlayerId>& killed, Position pos)
{
  // Not super effective but MAX_CLIENTS is 25 so this is theoretically O(1).
  for (const auto& [id, pl_pos] : positions)
    if (pl_pos == pos) {
      killed.insert(id);
      killed_this_turn.insert(id);
    }
}

void RoboticServer::explode_in_radius(std::set<PlayerId>& killed,
                                      std::set<Position>& destroyed,
                                      Position pos, client_messages::Direction dir)
{
  // Note: `<= radius` as the bomb position itself is also affected.
  for (uint16_t i = 0; i <= radius; ++i) {
    Position next = do_move(pos, dir);
    kill_on_position(killed, pos);

    if (blocks.contains(pos)) {
      destroyed.insert(pos);
      destroyed_this_turn.insert(pos);
      return;
    }

    if (next == pos)
      return;

    pos = next;
  }
}

Position RoboticServer::do_move(Position pos, client_messages::Direction dir) const
{
  using namespace client_messages;

  return std::visit([this, pos] <typename D> (D) {
      auto [x, y] = pos;
      if constexpr(std::same_as<D, Up>) {
        return (y + 1 < size_y) ? Position{x, y + 1} : pos;
      } else if constexpr(std::same_as<D, Down>) {
        return (y > 0) ? Position{x, y - 1} : pos;
      } else if constexpr(std::same_as<D, Left>) {
        return (x > 0) ? Position{x - 1, y} : pos;
      } else if constexpr(std::same_as<D, Right>) {
        return (x + 1 < size_x) ? Position{x + 1, y} : pos;
      } else {
        static_assert(always_false_v<D>, "Non-exhaustive pattern matching!");
      }
    }, dir);
}

server_messages::Turn RoboticServer::start_game()
{
  dbg("[game_master] Starting the game, cleaning all data and composing turn0.");
  killed_this_turn = {};
  positions = {};
  bombs = {};
  scores = {};
  blocks = {};
  explosions = {};

  server_messages::Turn turn{0, {}};
  auto& [_turnno, events] = turn;
  for (const auto& [plid, _] : players) {
    scores[plid] = 0;
    dbg("[game_master] Placing robots on the board.");
    Position pos = {rand() % size_x, rand() % size_y};
    positions[plid] = pos;
    events.push_back(server_messages::PlayerMoved{plid, pos});
  }

  for (uint16_t i = 0; i < initial_blocks; ++i) {
    dbg("[game_master] Placing blocks on the board.");
    Position pos = {rand() % size_x, rand() % size_y};
    blocks.insert(pos);
    events.push_back(server_messages::BlockPlaced{pos});
  }

  return turn;
}

void RoboticServer::end_game()
{
  std::cout << "GAME ENDED!!!\n";

  for (auto [id, score] : scores)
    std::cout << static_cast<int>(id) << "\t" << players.at(id).first
         << "@" << players.at(id).second << " got killed " << score << " times!\n";

  send_to_all(ServerMessage{scores});
  // Do not need a mutex for players as no thread will access it during game.
  players = {};
  {
    // I need a lock here though as client_handler may try to access this.
    std::lock_guard<std::mutex> lk{playing_clients_mutex};
    playing_clients = {};
  }

  for (size_t i = 0; i < clients.size(); ++i) {
    std::optional<ConnectedClient>& cm = clients.at(i);
    std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
    if (cm.has_value()) {
      cm->in_game = false;
    }
  }
}

// Thread functions.
void RoboticServer::acceptor()
{
  dbg("[acceptor] hello");
  for (;;) {
    if (number_of_clients > MAX_CLIENTS)
      throw ServerLogicError{
        "Number of connected clients shouldn't exceed the max number of clients!"
      };

    if (number_of_clients == MAX_CLIENTS) {
      dbg("[acceptor] No place for new clients, waiting for disconnections.");
      std::unique_lock lk{acceptor_mutex};
      for_places.wait(lk, [this] {return number_of_clients < MAX_CLIENTS;});
    }

    tcp::socket new_client(io_ctx);
    tcp_acceptor.accept(new_client);
    dbg("[acceptor] Accepted new client ", new_client.remote_endpoint().address(),
        ":", new_client.remote_endpoint().port());
    try {
      hail(new_client);
    } catch (std::exception& e) {
      // Disconnected during hailing? We ignore this connection then.
      dbg("[acceptor] Failed to hail the new client, disconnecting.");
      continue;
    }

    // Having hailed the client find him a place in the array.
    // There should be a place as number_of_clients has been checked beforehand.
    ConnectedClient cl{std::move(new_client), false, {}, 0};
    for (size_t i = 0; i < clients.size(); ++i) {
      if (clients.at(i).has_value())
        continue;

      ++number_of_clients;
      // todo do not need a mutex here as it is none?
      clients.at(i) = std::move(cl);
      // todo: consider reusing threads and simply giving them a blocking queue
      // with conn player? and they find the place in array? idk
      std::jthread th{[this, i] { client_handler(i); }};
      // We detach the client handling thread as its execution is independent.
      th.detach();
      break;
    }
  }
}

void RoboticServer::client_handler(size_t i)
{
  std::string addr;
  {
    std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
    addr = address_from_sock(clients.at(i)->sock);
  }
  dbg("[client_handler] Handling client ", i, " from ", addr);
  using namespace client_messages;
  Deserialiser<ReaderTCP> deser{clients.at(i)->sock};
  for (;;) {
    try {
      ClientMessage msg;
      deser >> msg;
      {
        std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
        std::visit([this, i, &addr] <typename Cm> (const Cm& cm) {
            if constexpr(std::same_as<Cm, Join>) {
              if (!clients.at(i)->in_game && lobby) {
                // Do this only when in lobby state, do not bother join handler.
                joined.push({i, {cm, addr}});
              }
            } else if (!lobby) {
              // Stray moves in the lobby should not affect the upcoming game.
              clients.at(i)->current_move = cm;
            }
          }, msg);
      }
    } catch (std::exception& e) {
      // Upon disconnection this thread says au revoir.
      dbg("[client_handler] Something bad happened: ", e.what());
      dbg("[client_handler] disconnecting client ", addr);
      {
        std::lock_guard<std::mutex> lk{playing_clients_mutex};
        playing_clients.erase(clients.at(i)->id);
      }
      {
        std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
        clients.at(i) = {};
      }
      --number_of_clients;
      for_places.notify_all();
      return;
    }
  }
}

void RoboticServer::join_handler()
{
  dbg("[join_handler] hello");
  for (;;) {
    // all players are here...
    if (lobby && players.size() == players_count) {
      dbg("[join_handler] Required number of players joined, waking up the gm.");
      lobby = false;
      // wake up the game master, he has waited enough did he not
      for_game.notify_all();
    }

    auto [i, player] = joined.pop();

    dbg("[join_handler] popped client ", i, " ie ", player.first, "@", player.second);

    if (!lobby)
      continue;

    bool accepted = false;
    uint8_t id;
    {
      std::lock_guard<std::mutex> lk{clients_mutices.at(i)};
      if (clients.at(i).has_value() && !clients.at(i)->in_game) {
        // todo: this should only be set by the game master?
        clients.at(i)->in_game = true;
        // todo: i am locking (TWICE!!) while under client mutex, check deadlockiness
        {
          std::lock_guard<std::mutex> lk{players_mutex};
          id = get_free_id(players);
          players.insert({id, player});
        }
        {
          std::lock_guard<std::mutex> lk{playing_clients_mutex};
          playing_clients[id] = i;
        }
        clients.at(i)->id = id;
        dbg("[join_handler] accepting this clients Join, their id: ", static_cast<int>(id));
        accepted = true;
      }
    }

    if (accepted)
      send_to_all(ServerMessage{server_messages::AcceptedPlayer{id, player}});
  }
}

void RoboticServer::game_master()
{
  dbg("[game_master] hello");
  size_t turn_number = 0;
  for (;;) {
    server_messages::Turn current_turn{turn_number, {}};
    if (turn_number == game_len || lobby) {
      dbg("[game_master] Lobby, going to wait for players.");
      lobby = true;
      std::unique_lock lk{game_master_mutex};
      for_game.wait(lk, [this] { return !lobby; });
      dbg("[game_master] Just woken up, starting a game.");
      // We are awake, out of lobby. Let's get this going then shall we.
      current_turn = turn0 = start_game();
      turn_number = 0;
    }

    // todo: should i wait when turnno = 0? perhaps send the turn straight away
    dbg("[game_master] Waiting for ", turn_duration, "ms...");
    std::this_thread::sleep_for(std::chrono::milliseconds(turn_duration));

    if (turn_number > 0) {
      killed_this_turn = {};
      destroyed_this_turn = {};
      do_bombing(current_turn);
      gather_moves(current_turn);

      for (PlayerId id : killed_this_turn) {
        dbg("[game master] Player ", static_cast<int>(id),
            " got killed this turn, new place for them");
        Position pos = {rand() % size_x, rand() % size_y};
        positions[id] = pos;
        current_turn.second.push_back(server_messages::PlayerMoved{id, pos});
      }

      std::lock_guard<std::mutex> lk{turn0_mutex};
      // append those events to turn0
      for (const server_messages::Event& ev : current_turn.second)
        turn0.second.push_back(ev);
    }

    // todo send it to clients
    dbg("[game_master] Turn ", current_turn.first);
    dbg("[game_master] Sending ", current_turn.second.size(), " events to clients", "\n");
    send_to_all(ServerMessage{current_turn});

    for (PlayerId id : killed_this_turn) {
      ++scores.at(id);
    }

    // Erase blocks that got destroyed.
    for (Position pos : destroyed_this_turn)
      blocks.erase(pos);

    ++turn_number;
    if (turn_number == game_len)
      end_game();
  }
}

// Main server function.
void RoboticServer::run()
{
  // todo: should i run one of these in the main thread?
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
    uint16_t players_count;
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
      ("players-count,c", po::value<uint16_t>(&players_count)->required())
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

    if (players_count > std::numeric_limits<uint8_t>::max()) {
      throw ServerError{"players-count must fit in one byte!"};
    }

    RoboticServer server{name, timer, static_cast<uint8_t>(players_count),
      turn_duration, radius, initial_blocks,
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
