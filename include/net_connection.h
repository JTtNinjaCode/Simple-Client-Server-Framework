#pragma once
#include "net_common.h"
#include "net_message.h"
#include "net_ts_queue.h"

namespace net {
template <typename T>
class ServerInterface;

template <typename T>
class Connection : public std::enable_shared_from_this<Connection<T>> {
 public:
  enum class Owner {
    kServer,
    kClient,
  };

  Connection(Owner Owner, asio::io_context& asio_context,
             asio::ip::tcp::socket&& socket,
             TsQueue<OwnedMessage<T>>& message_in)
      : owner_(Owner),
        asio_context_(asio_context),
        socket_(std::move(socket)),
        message_in_(message_in) {}

  virtual ~Connection() { Disconnect(); }

  // [Client] Connect to server and call ReadValidation to validate that this
  // connection is legitimate
  void ConnectToServer(asio::ip::tcp::resolver::results_type& endpoints) {
    if (owner_ == Owner::kClient) {
      asio::async_connect(
          socket_, endpoints,
          [this](system::error_code ec, asio::ip::tcp::endpoint endpoint) {
            if (!ec) {
              std::cout << "[Client] Connect Success.\n";
              ReadValidation();
            } else if (ec == asio::error::eof) {
              std::cout << "[" << id_ << "] socket has been terminated\n";
              socket_.close();
            } else {
              std::cerr << "[Client] Connect Failed.\n";
              socket_.close();
            }
          });
    }
  }

  // [Server] Connect to client, assign an ID and call WriteValidation to
  // validate that client connection is legitimate
  void ConnectToClient(uint32_t uid) {
    if (owner_ == Owner::kServer) {
      if (socket_.is_open()) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<int64_t> dis(
            std::numeric_limits<int64_t>::min(),
            std::numeric_limits<int64_t>::max());
        handshake_out_ = dis(gen);
        handshake_check_ = Scramble(handshake_out_);
        id_ = uid;
        WriteValidation();
      }
    }
  }

  uint32_t GetID() { return id_; }

  asio::ip::tcp::socket& GetSocket() { return socket_; }

  // [Client, Server]
  void Disconnect() {
    if (IsConnected()) asio::post(asio_context_, [this]() { socket_.close(); });
  }

  // [Client, Server]
  bool IsConnected() const { return socket_.is_open(); }

  // [Client, Server]
  void Send(const Message<T>& msg) {
    asio::post(asio_context_, [this, msg]() {
      // If the queue has a message in it, then we must
      // assume that it is in the process of asynchronously being written.
      // Either way add the message to the queue to be output. If no messages
      // were available to be written, then start the process of writing the
      // message at the front of the queue.
      bool writing_msg = !message_out_.empty();
      message_out_.push_back(msg);
      if (!writing_msg) {
        WriteHeader();
      }
    });
  }

 private:
  // [Client, Server]
  void ReadHeader() {
    asio::async_read(
        socket_, asio::buffer(&temp_msg_.header, sizeof(MessageHeader<T>)),
        [this](system::error_code ec, std::size_t length) {
          if (!ec) {
            if (temp_msg_.data_size() > 0) {
              temp_msg_.body.resize(temp_msg_.data_size());
              ReadBody();
            } else {  // complete
              AddToIncomingMessageQueue();
            }
          } else if (ec == asio::error::eof) {
            std::cout << "[" << id_ << "] socket has been terminated\n";
            socket_.close();
          } else {
            std::cerr << "[" << id_ << "] async_read error.\n";
            socket_.close();
          }
        });
  }

  // [Client, Server]
  void ReadBody() {
    asio::async_read(
        socket_, asio::buffer(temp_msg_.data_addr(), temp_msg_.data_size()),
        [this](system::error_code ec, std::size_t length) {
          if (!ec) {
            AddToIncomingMessageQueue();
          } else if (ec == asio::error::eof) {
            std::cout << "[" << id_ << "] socket has been terminated\n";
            socket_.close();
          } else {
            std::cerr << "[" << id_ << "] async_read error.\n";
            socket_.close();
          }
        });
  }

  // [Client, Server]
  void WriteHeader() {
    Message<T>& msg = message_out_.front();
    asio::async_write(
        socket_, asio::buffer(msg.header_addr(), msg.header_size()),
        [this](system::error_code ec, std::size_t length) {
          if (!ec) {
            if (message_out_.front().data_size() > 0) {
              WriteBody();
            } else {
              message_out_.pop_front();
              if (!message_out_.empty()) {
                WriteHeader();
              }
            }
          } else if (ec == asio::error::eof) {
            std::cout << "[" << id_ << "] socket has been terminated\n";
            socket_.close();
          } else {
            // Write Message Header Fail
            std::cout << "[" << id_ << "] Write Message Header Failed.\n";
            socket_.close();
          }
        });
  }

  // [Client, Server]
  void WriteBody() {
    asio::async_write(
        socket_,
        asio::buffer((void*)message_out_.front().data_addr(),
                     message_out_.front().data_size()),
        [this](system::error_code ec, std::size_t length) {
          if (!ec) {
            message_out_.pop_front();
            if (!message_out_.empty()) {
              WriteHeader();
            }
          } else if (ec == asio::error::eof) {
            std::cout << "[" << id_ << "] socket has been terminated\n";
            socket_.close();
          } else {
            // Write Message Header Fail
            std::cout << "[" << id_ << "] Write Message Header Failed.\n";
            socket_.close();
          }
        });
  }

  // [Client, Server]
  void AddToIncomingMessageQueue() {
    if (owner_ == Owner::kServer) {
      message_in_.push_back({this->shared_from_this(), temp_msg_});
    } else if (owner_ == Owner::kClient) {
      message_in_.push_back({nullptr, temp_msg_});
    }
    ReadHeader();
  }

  // [Client, Server]
  uint64_t Scramble(uint64_t input) {
    uint64_t out = input ^ 0xdeadbeefdeadbeef;
    out = (out & 0xf0f0f0f0f0f0f0) >> 4 | (out & 0xf0f0f0f0f0f0f0) << 4;
    return out ^ 0xbeef12345678dead;
  }

  // [Client, Server]
  void WriteValidation() {
    asio::async_write(socket_, asio::buffer(&handshake_out_, sizeof(uint64_t)),
                      [this](system::error_code ec, std::size_t length) {
                        if (!ec) {
                          if (owner_ == Owner::kServer) {
                            ReadValidation();
                          } else if (owner_ == Owner::kClient) {
                            ReadHeader();
                          }
                        } else if (ec == asio::error::eof) {
                          std::cout << "[" << id_
                                    << "] socket has been terminated\n";
                          socket_.close();
                        } else {
                          std::cerr << "[------] write validation error.\n";
                          socket_.close();
                        }
                      });
  }

  // [Client, Server]
  void ReadValidation() {
    asio::async_read(
        socket_, asio::buffer(&handshake_in_, sizeof(uint64_t)),
        [this /*, server*/](system::error_code ec, std::size_t length) {
          if (!ec) {
            if (owner_ == Owner::kServer) {
              if (handshake_in_ == handshake_check_) {
                std::cout << "[Server] Client Validation Success.\n";
                ReadHeader();
              } else {
                std::cerr << "[Server] Client Validation Failure.\n";
                socket_.close();
              }
            } else if (owner_ == Owner::kClient) {
              handshake_out_ = Scramble(handshake_in_);
              WriteValidation();
            }
          } else if (ec == asio::error::eof) {
            std::cout << "[" << id_ << "] socket has been terminated\n";
            socket_.close();
          } else {
            std::cerr << "[------] read validation error.\n";
            socket_.close();
          }
        });
  }

 protected:
  // each connection has a unique socket to remote
  asio::ip::tcp::socket socket_;
  // this context is shared with the whole asio instance
  asio::io_context& asio_context_;
  // owner decide how some of the connection behaves
  Owner owner_;
  uint32_t id_;

  Message<T> temp_msg_;

  TsQueue<Message<T>> message_out_;
  TsQueue<OwnedMessage<T>>& message_in_;

  // validation
  uint64_t handshake_in_;
  uint64_t handshake_out_;
  uint64_t handshake_check_;
};
}  // namespace net