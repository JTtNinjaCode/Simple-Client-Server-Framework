#include "net_client.h"
enum Operation {
  // [Client] Send the current local time to the server.
  // [Server] Retrieve the client's time and echo it back.
  kPing,
  // [Client] Send a string to the server and print it out on the server's
  // terminal.
  // [Server] Send a string to the client and print it out on the client's
  // terminal.
  kRemotePrint,
  // [Client] Send a string to the server and have the server broadcast it to
  // every client.
  // [Server] Broadcast a string from the client to every other client (except
  // the source client). Alternatively, the server itself can initiate a
  // broadcast to all clients.
  kBroadcast,
};

class ClientTest : public net::ClientInterface<Operation> {
 public:
  void Ping() {
    net::Message<Operation> msg(Operation::kPing);
    std::chrono::system_clock::time_point time_now =
        std::chrono::system_clock::now();
    msg << time_now;
    connection_->Send(msg);
  }

  void SendString() {
    net::Message<Operation> msg(Operation::kRemotePrint);
    std::string send_str = "Hello, I'm a Client.";
    msg << send_str;
    connection_->Send(msg);
  }

  void Broadcast() {
    net::Message<Operation> msg(Operation::kBroadcast);
    std::string str = "Hello, everyone.";
    msg << str;
    connection_->Send(msg);
  }
};

int main() {
  ClientTest client;
  client.Connect("127.0.0.1", 60000);

  static bool key[4] = {false, false, false, false};
  static bool old_key[4] = {false, false, false, false};

  bool quit = false;
  std::cout << R"(Press 1 to ping the server.)" << '\n';
  std::cout << R"(Press 2 to send a message "Hello, I'm a Client." to the server.)"  << '\n';
  std::cout << R"(Press 3 to broadcast message: Hello, everyone.)" << '\n';
  std::cout << R"(Press 4 to close.)" << '\n';

  while (!quit) {
    // update key state
    if (GetForegroundWindow() == GetConsoleWindow()) {
      key[0] = GetAsyncKeyState('1') & 0x8000;  // ping server
      key[1] = GetAsyncKeyState('2') & 0x8000;  // send string
      key[2] = GetAsyncKeyState('3') & 0x8000;  // broadcast
      key[3] = GetAsyncKeyState('4') & 0x8000;  // quit
    }
    // if key is true, but old_key is false, it indicates that the key has just
    // been pressed.
    if (key[0] && !old_key[0])
      client.Ping();
    else if (key[1] && !old_key[1])
      client.SendString();
    else if (key[2] && !old_key[2])
      client.Broadcast();
    else if (key[3] && !old_key[3])
      quit = true;

    for (size_t i = 0; i < 3; i++) {
      old_key[i] = key[i];
    }

    if (client.IsConnected()) {
      // Polling: If a message arrives, process it.
      if (!client.IncomingQueue().empty()) {
        auto msg = client.IncomingQueue().pop_front().msg;
        switch (msg.header.op) {
          case Operation::kPing: {
            std::chrono::system_clock::time_point time_now =
                std::chrono::system_clock::now();
            std::chrono::system_clock::time_point time_then;
            msg >> time_then;
            std::cout << std::chrono::duration<double>(time_now - time_then).count() << '\n';
          } break;
          case Operation::kRemotePrint: {
            std::string str;
            msg >> str;
            std::cout << str << '\n';
          } break;
          case Operation::kBroadcast: {
            std::string str;
            msg >> str;
            std::cout << str << '\n';
          } break;
          default: {
          }
        }
      }
    } else {
      std::cout << "[Client] Cant connect to the server.\n";
      quit = true;
    }
  }
  return 0;
}