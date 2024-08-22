#pragma once
#include "net_common.h"

namespace net {
template <typename T>
struct MessageHeader {
  T op{};
  uint32_t data_size = 0;
};

// Message operates like a stack-like
template <typename T>
struct Message {
  Message() = default;
  Message(T op) { header.op = op; }

  size_t entire_size() const { return sizeof(header) + body.size(); }
  size_t header_size() const { return sizeof(header); }
  size_t data_size() const { return header.data_size; }
  void* data_addr() { return body.data(); }
  void* header_addr() { return &header; }
  void set_op(T op) { header.op = op; }
  T get_op() { return header.op; }

  friend std::ostream& operator<<(std::ostream& os, const Message<T>& msg) {
    os << "[Message] Operation: " << int(msg.header.op)
       << ", Size: " << msg.size();
    return os;
  }

  template <typename DataType>
  friend Message<T>& operator<<(Message<T>& msg, const DataType& data) {
    static_assert(std::is_standard_layout_v<DataType>, "Data is too complex");
    size_t curr_size = msg.body.size();
    msg.body.resize(msg.body.size() + sizeof(DataType));
    std::memcpy(msg.body.data() + curr_size, &data, sizeof(DataType));
    msg.header.data_size = msg.body.size();
    return msg;
  }

  // Specialization: std::string, write the contents of a string
  // (NULL-terminated) and its length(excluding NULL terminator).
  friend Message<T>& operator<<(Message<T>& msg, const std::string& str) {
    size_t curr_size = msg.body.size();
    msg.body.resize(msg.body.size() + str.length() + 1 + sizeof(size_t));
    std::memcpy(msg.body.data() + curr_size, str.data(), str.length());
    curr_size += str.length() + 1;
    size_t str_len = str.length();
    std::memcpy(msg.body.data() + curr_size, &str_len, sizeof(size_t));
    msg.header.data_size = msg.body.size();
    return msg;
  }

  template <typename DataType>
  friend Message<T>& operator>>(Message<T>& msg, DataType& data) {
    static_assert(std::is_trivially_copyable_v<DataType>,
                  "Data is too complex");
    size_t new_size = msg.body.size() - sizeof(DataType);
    std::memcpy(&data, msg.body.data() + new_size, sizeof(DataType));
    msg.body.resize(new_size);
    msg.header.data_size = new_size;
    return msg;
  }

  // Specialization: std::string, read the length of a string(excluding NULL
  // terminator) and its contents(NULL-terminated)
  friend Message<T>& operator>>(Message<T>& msg, std::string& str) {
    size_t str_len = 0;
    size_t new_size = msg.body.size() - sizeof(size_t);
    std::memcpy(&str_len, msg.body.data() + new_size, sizeof(size_t));
    msg.body.resize(new_size);

    str.resize(str_len + 1);
    new_size = msg.body.size() - (str_len + 1);
    std::memcpy((void*)str.data(), msg.body.data() + new_size, str_len + 1);
    msg.body.resize(new_size);
    return msg;
  }

  MessageHeader<T> header;
  std::vector<uint8_t> body;
};

template <typename T>
class Connection;

template <typename T>
struct OwnedMessage {
  std::shared_ptr<Connection<T>> remote = nullptr;
  Message<T> msg;
  friend std::ostream& operator<<(std::ostream& os,
                                  const OwnedMessage<T>& msg) {
    os << msg;
    return os;
  }
};
}  // namespace net