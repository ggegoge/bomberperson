
#include "sockets.h"
#include "netio.h"
#include "serialise.h"
#include "messages.h"

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <string>

using namespace std;

ostream& operator<<(ostream& os, vector<uint8_t>& bytes)
{
  os << "[ ";

  for (uint8_t b : bytes)
    os << (int)b << " ";

  os << "]";
  return os;
}

void send_test(SocketUDP& sock, const string& opt);
void recv_test(SocketUDP& sock, const string& opt);

int main(int argc, char* argv[])
{
  if (argc < 6) {
    printf("Usage: %s <port> <host> <host-port> <send/recv> <opt>\n", argv[0]);
    exit(1);
  } else {
    printf("argc == %d\n", argc);
  }

  uint16_t port = std::stol(argv[1]);
  char* host = argv[2];
  uint16_t host_port = std::stol(argv[3]);
  SocketUDP sock(port, host, host_port);

  Serialiser ser;
  std::vector<uint8_t> bytes;
  string send_recv(argv[4]);
  std::string opt(argv[5]);

  if (send_recv == "send")
    send_test(sock, opt);
  else if (send_recv == "recv")
    recv_test(sock, opt);
  else
    cout << "send/recv must be either send or recv\n";

  
  return 0;
}

#include "dbg.h"
void recv_test(SocketUDP& sock, const string& opt)
{
  ReaderUDP rd;
  rd.recv_from_sock(sock);

  Deserialiser<ReaderUDP> deser{rd};

  if (opt == "client") {
    // using namespace client_messages;
    // ClientMessage msg;
    // deser >> msg;
  } else if (opt == "server") {
    using namespace server_messages;
    ServerMessage msg;
    deser >> msg;

    cout << msg;
  } else if (opt == "input") {

  } else {
    cout << "invalid opt!\n";
  }
}

void send_test(SocketUDP& sock, const string& opt)
{
  Serialiser ser;
  std::vector<uint8_t> bytes;

    if (opt == "client") {
    using namespace client_messages;
    
    // wyślemy Join("siemkaaa") -> [0, 8, s, i, ..., a]
    struct client_messages::Join j("siemkaaa");
    ClientMessage msg = j;
    ser << msg;
    bytes = ser.to_bytes();
    cout << bytes << "\n";
    sock.send_message(bytes);
    ser.clean();

    // Teraz PlaceBomb
    ser << client_messages::PlaceBomb;
    bytes = ser.to_bytes();
    cout << bytes << "\n";
    sock.send_message(bytes);
    ser.clean();

    // Teraz PlaceBlock
    struct PlaceBlock pb;
    msg = pb;
    ser << msg;
    bytes = ser.to_bytes();
    cout << bytes << "\n";
    sock.send_message(bytes);
    ser.clean();

    // Teraz Move(Left)
    struct client_messages::Move m(client_messages::Right);
    msg = m;
    ser << msg;
    bytes = ser.to_bytes();
    cout << bytes << "\n";
    sock.send_message(bytes);
    ser.clean();
  } else if (opt == "server") {
    using namespace server_messages;

    ServerMessage msg;
    
    // sample Hello messagea
    struct Hello hello;
    hello.server_name = "goowno";
    hello.players_count = 21;
    hello.size_x = 12;
    hello.size_y = 34;
    hello.game_length = 2137;
    hello.explosion_radius = 13;
    hello.bomb_timer = 4;

    msg = hello;
    ser << msg;
    bytes = ser.to_bytes();
    cout << bytes << "\n";
    sock.send_message(bytes);
    ser.clean();

    // sample Turn: PMoved, BoPlaced, Bexploded
    EventVar e1, e2, e3;
    
    struct PlayerMoved pm;
    pm.id = 7;
    pm.position = {21, 37};
    e1 = pm;

    struct BombPlaced bp;
    bp.id = 113;
    bp.position = {73, 12};
    e2 = bp;

    struct BombExploded be;
    be.id = 77;
    be.killed = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    be.blocks_destroyed = {{0, 0}, {1,1}, {2,2}};
    e3 = be;
    
    vector<EventVar> evs = {e1, e2, e3};
    struct Turn turn;
    turn.turn = 2137;
    turn.events = evs;

    msg = turn;
    ser.clean();
    ser << msg;
    bytes = ser.to_bytes();
    cout << bytes << "\n";
    sock.send_message(bytes);
    ser.clean();
    
  } else if (opt == "lobby") {
    using namespace display_messages;
    DisplayMessage msg;
    struct Lobby l;
    l.server_name = "gniox";
    l.players_count = 11;
    l.size_x = 1234;
    l.size_y = 1;
    l.game_length = 0;
    l.explosion_radius = 11;
    l.bomb_timer = 17;
    struct server_messages::Player p1;
    p1.name = "kot";
    p1.address = "1.2.3.4:0001";
    struct server_messages::Player p2;
    p2.name = "ja";
    p2.address = "4.3.2.1:2137";
    struct server_messages::Player p3;
    p3.name = "pojscie spac";
    p3.address = "5.7.3.8:0001";    
    l.players.insert({1, p1});
    l.players.insert({2, p2});
    l.players.insert({3, p3});
    
    msg = l;
    ser.clean();
    ser << msg;
    bytes = ser.to_bytes();
    cout << bytes << "\n";
    sock.send_message(bytes);
    ser.clean();
  } else if (opt == "game") {
    using namespace display_messages;
    DisplayMessage msg;
    struct Game g;
    g.server_name = "gniox";
    g.size_x = 10;
    g.size_y = 10;
    g.game_length = 0;
    g.turn = 11;

    struct server_messages::Player p1;
    p1.name = "kot";
    p1.address = "1.2.3.4:0001";
    struct server_messages::Player p2;
    p2.name = "ja";
    p2.address = "4.3.2.1:2137";
    struct server_messages::Player p3;
    p3.name = "pójście spać";
    p3.address = "5.7.3.8:0001";    

    g.players.insert({1, p1});
    g.players.insert({2, p2});
    g.players.insert({3, p3});

    g.player_positions.insert({1, {1,1}});
    g.player_positions.insert({2, {4,4}});
    g.player_positions.insert({3, {7,7}});

    g.blocks = {{0,0}, {5, 5}};

    struct server_messages::Bomb b;
    b.position = {9,9};
    b.timer = 3;

    g.bombs = {b};

    g.explosions = {{1, 7}, {7, 1}};
    
    g.scores.insert({1, 10});
    g.scores.insert({2, 0});
    g.scores.insert({3, 55});

    msg = g;
    ser.clean();
    ser << msg;
    bytes = ser.to_bytes();
    cout << bytes << "\n";
    sock.send_message(bytes);
    ser.clean();
  } else if (opt == "input") {
    using namespace input_messages;
    struct client_messages::Move m(client_messages::Right);
    InputMessage msg = m;
    ser << msg;
    bytes = ser.to_bytes();
    sock.send_message(bytes);
    ser.clean();
  } else {
    cout << "wrong opt!\n";
  }
}
