
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
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <math.h>
#include <regex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace po = boost::program_options;

using boost::asio::ip::udp;
using boost::asio::ip::tcp;

using boost::asio::ip::resolver_base;

using namespace std;

class ClientError : public std::runtime_error {
public:
  ClientError()
    : std::runtime_error("Client error!") {}

  ClientError(const std::string& msg) : std::runtime_error(msg) {}
};


namespace {

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
  uint16_t portnum;
  tcp::socket server_socket;
  udp::socket gui_socket;
  udp::endpoint gui;
  tcp::endpoint server;
  Serialiser ser;
  Deserialiser<ReaderTCP> server_deser;
  Deserialiser<ReaderUDP> gui_deser;
public:
  RoboticClient(const string& name, uint16_t portnum,
                const string& server_addr, const string& gui_addr)
    : name(name), portnum(portnum), server_socket(io_ctx),
      gui_socket(io_ctx, udp::endpoint(udp::v6(), portnum)),
      gui(), server(), server_deser(server_socket), gui_deser({})
  {
    auto [gui_ip, gui_port] = get_addr(gui_addr);
    cout << "gui_ip=" << gui_ip << ", " << "gui_port=" << gui_port << "\n";
    udp::resolver udp_resolver(io_ctx);
    gui = *udp_resolver.resolve(gui_ip, gui_port, resolver_base::numeric_service);
    gui_socket.connect(gui);

    auto [serv_ip, serv_port] = get_addr(server_addr);
    cout << "serv_ip=" << serv_ip << ", " << "serv_port=" << serv_port << "\n";
    tcp::resolver tcp_resolver(io_ctx);
    server = *tcp_resolver.resolve(serv_ip, serv_port, resolver_base::numeric_service);

    server_socket.connect(server);
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
    ser.clean();
    ser << msg;
    vector<uint8_t> bytes = ser.drain_bytes();
    gui_socket.send(boost::asio::buffer(bytes));

    using namespace client_messages;
    // wyślemy Join("siemkaaa") -> [0, 8, s, i, ..., a]
    struct Join j("siemkaaa");
    ClientMessage send_msg = j;
    ser << send_msg;
    bytes = ser.drain_bytes();
    server_socket.send(boost::asio::buffer(bytes));
  }
};

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

