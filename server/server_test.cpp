#include "net_server.h"
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

class ServerTest : public net::ServerInterface<Operation> {
 public:
  ServerTest(uint16_t port) : net::ServerInterface<Operation>(port) {}

 protected:
  virtual bool OnClientConnect(
      std::shared_ptr<net::Connection<Operation>> client) {
    net::Message<Operation> message(Operation::kRemotePrint);
    std::cout << "[Server] Allow Connection: "
              << client->GetSocket().remote_endpoint() << '\n';
    return true;
  }
  virtual void OnClientDisconnect(
      std::shared_ptr<net::Connection<Operation>> client) {
    std::cout << "[Server] Client " << client->GetID() << " disconnect.\n";
  }
  virtual void OnMessageArrive(
      std::shared_ptr<net::Connection<Operation>> client,
      net::Message<Operation>& message) {
    switch (message.header.op) {
      case Operation::kPing: {
        std::cout << "[Server] Ping from client: " << client->GetID() << '\n';
        SendClient(client, message);
      } break;
      case Operation::kRemotePrint: {
        std::string str;
        message >> str;
        std::cout << "[Server] Client " << client->GetID()
                  << " Message: " << str << '\n';
      } break;
      case Operation::kBroadcast: {
        std::string str;
        message >> str;
        std::cout << "[Server] Client " << client->GetID()
                  << " Broadcast Message: " << str << '\n';
        message << str;
        SendAllClient(message, client);
      } break;
    }
  }
};

int main() {
  ServerTest server(60000);  // open 60000 port and listen
  server.Start();
  while (true) {
    server.Update();
  }
  return 0;
}