#pragma once
#include "net_common.h"
#include "net_connection.h"
#include "net_message.h"
#include "net_ts_queue.h"
namespace net {
template <typename T>
class ClientInterface {
 public:
  // 初始化 Socket
  ClientInterface() : socket_(asio_context_) {}
  ~ClientInterface() { Disconnect(); }
  // 回傳成功或失敗
  bool Connect(const std::string& host, const uint16_t port) {
    try {
      // The resolver is used to convert human-readable endpoints (like domain
      // names and service names) into IP addresses and port numbers that can be
      // used for network connections

      // The resolver returns multiple results because a single host name may be
      // associated with multiple IP addresses. This can happen due to reasons
      // such as load balancing, support for multiple protocols (returning both
      // IPv4 and IPv6 addresses), among others.
      asio::ip::tcp::resolver resolver(asio_context_);
      auto endpoints = resolver.resolve(host, std::to_string(port));
      connection_ = std::make_unique<Connection<T>>(
          Connection<T>::Owner::kClient, asio_context_,
          asio::ip::tcp::socket(asio_context_), message_in_);

      connection_->ConnectToServer(endpoints);

      context_thread_ = std::thread([this]() { asio_context_.run(); });
    } catch (std::exception& e) {
      std::cerr << e.what() << '\n';
      return false;
    }
    return true;
  }

  void Disconnect() {
    if (IsConnected()) {
      connection_->Disconnect();
    }

    asio_context_.stop();
    if (context_thread_.joinable()) {
      context_thread_.join();
    }

    connection_.release();
  }

  bool IsConnected() {
    if (connection_) {
      return connection_->IsConnected();
    } else {
      return false;
    }
  }

  TsQueue<OwnedMessage<T>>& IncomingQueue() { return message_in_; }

 protected:
  // asio context handle the data transfer
  asio::io_context asio_context_;
  // but need a thread of its own to execute its work command
  std::thread context_thread_;

  asio::ip::tcp::socket socket_;
  std::unique_ptr<Connection<T>> connection_;

 private:
  // 其實只要 Message<T> 即可，但為了保持 Connection 的結果一致只能用
  // OwnedMessage
  TsQueue<OwnedMessage<T>> message_in_;
};
}  // namespace net