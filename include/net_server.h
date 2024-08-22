#pragma once
#include "net_common.h"
#include "net_connection.h"
#include "net_message.h"

namespace net {
template <typename T>
class ServerInterface {
 public:
  // Create the asio_context_ and the acceptor (which is essentially a socket
  // responsible for using async_accept to create sockets for connecting with
  // various clients).
  ServerInterface(uint16_t port)
      // In fact:
      // - asio relies on WinSock2.h.(in windows.)
      // - asio::ip::tcp::v4() corresponds to the AF_INET macro
      // - asio::ip::tcp::endpoint is essentially sockaddr_in
      // - asio::ip::tcp::acceptor is effectively equivalent to calling socket,
      //   bind, listen, and other preparatory steps, enabling direct listening.
      : asio_acceptor_(asio_context_,
                       asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {}

  virtual ~ServerInterface() { Stop(); }

  // Start continuously accepting clients, creating sockets, and wrapping them
  // into Connections.
  void Start() {
    try {
      thread_context_ = std::thread([this]() { asio_context_.run(); });
      WaitForClientConnection();
      std::cout << "[Server] Started." << std::endl;
    } catch (std::exception& e) {
      std::cerr << "[Server] Exception:" << e.what() << '\n';
    }
  }

  void Stop() {
    asio_context_.stop();
    if (thread_context_.joinable()) {
      thread_context_.join();
    }
    std::cout << "[Server] Stopped.\n";
  }

  void WaitForClientConnection() {
    asio_acceptor_.async_accept([this](std::error_code ec,
                                       asio::ip::tcp::socket socket) {
      if (!ec) {
        std::cout << "[Server] New Connection: " << socket.remote_endpoint()
                  << '\n';
        // wrap the socket into a connection and point to it using shared_ptr
        std::shared_ptr<Connection<T>> new_conn =
            std::make_shared<Connection<T>>(Connection<T>::Owner::kServer,
                                            asio_context_, std::move(socket),
                                            message_in_);
        // give the server a chance to deny connection
        if (OnClientConnect(new_conn)) {
          connections_.push_back(std::move(new_conn));
          connections_.back()->ConnectToClient(id_counter_++);
          std::cout << "[Server] Connection " << connections_.back()->GetID()
                    << " Approved\n";
        } else {
          std::cout << "[Server] Connection Denied\n";
        }
      } else {
        std::cout << "[Server] New Connection Error:" << ec.message() << '\n';
      }

      WaitForClientConnection();
    });
  }

  // Specify the number of messages to process.
  void Update(
      unsigned int max_messages = std::numeric_limits<unsigned int>::max()) {
    // block main thread until message_in_ is not empty
    message_in_.wait_until_non_empty();

    size_t message_count = 0;
    while (message_count < max_messages && !message_in_.empty()) {
      auto msg = message_in_.pop_front();
      OnMessageArrive(msg.remote, msg.msg);
      message_count++;
    }

    RemoveClient();
  }

  void RemoveClient() {
    bool invalid_client_exists = false;

    for (auto& client : connections_) {
      // if client == nullptr or disconnected
      if (!(client && client->IsConnected())) {
        OnClientDisconnect(client);
        client.reset();  // Set to nullptr to indicate that this client is dead.
        invalid_client_exists = true;
      }
    }

    // remove dead connection;
    if (invalid_client_exists) {
      // erase remove idiom
      connections_.erase(
          std::remove(connections_.begin(), connections_.end(), nullptr),
          connections_.end());
    }
  }

  // Send message to a specified client
  void SendClient(std::shared_ptr<Connection<T>> client,
                  const Message<T>& msg) {
    if (client && client->IsConnected()) {
      client->Send(msg);
    }
  }

  // Broadcast the message to all clients
  void SendAllClient(const Message<T>& msg,
                     std::shared_ptr<Connection<T>> ignore_client = nullptr) {
    for (auto& client : connections_) {
      if (client && client->IsConnected()) {
        if (client != ignore_client) {
          client->Send(msg);
        }
      }
    }
  }

  TsQueue<OwnedMessage<T>>& IncomingQueue() { return message_in_; }

 protected:
  friend Connection<T>;
  virtual bool OnClientConnect(std::shared_ptr<Connection<T>> client) {
    return false;
  }
  virtual void OnClientValidationSuccess(
      std::shared_ptr<Connection<T>> client) {}
  // After the client disconnects, the connection is not immediately removed
  // from connections_, it will only be removed when RemoveClient is called.
  virtual void OnClientDisconnect(std::shared_ptr<Connection<T>> client) {}
  virtual void OnMessageArrive(std::shared_ptr<Connection<T>> client,
                               Message<T>& message) {}

 protected:
  TsQueue<OwnedMessage<T>> message_in_;

  std::vector<std::shared_ptr<Connection<T>>> connections_;

  // asio_ontext must be placed before the acceptor due to class initialization
  // order.
  asio::io_context asio_context_;
  std::thread thread_context_;

  asio::ip::tcp::acceptor asio_acceptor_;
  uint32_t id_counter_ = 10000;
};
}  // namespace net