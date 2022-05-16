
#include "sockets.h"
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
#include <boost/program_options/errors.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <utility>
#include <vector>

namespace po = boost::program_options;

using boost::asio::ip::udp;
using boost::asio::ip::tcp;

using boost::asio::ip::resolver_base;

using namespace std;

// todo

namespace {

pair<string, string> get_addr(const string& addr)
{
  return {"::1", "40007"};
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
    udp::resolver udp_resolver(io_ctx);
    gui = *udp_resolver.resolve(gui_ip, gui_port, resolver_base::numeric_service);
    gui_socket.connect(gui);

    auto [serv_ip, serv_port] = get_addr(server_addr);
    tcp::resolver tcp_resolver(io_ctx);
    server = *tcp_resolver.resolve(serv_ip, serv_port, resolver_base::numeric_service);

    server_socket.connect(server);
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

  } catch (po::required_option& e) {
    cout << "Missing some options: " << e.what() << "\n";
    cout << "See " << argv[0] << " -h for help.\n";
    return 1;
  } catch (std::exception& e) {
    cout << e.what() << "\n";
    return 1;
  }

  return 0;
}

