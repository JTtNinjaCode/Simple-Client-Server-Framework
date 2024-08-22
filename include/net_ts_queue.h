#pragma once
#include "net_common.h"
namespace net {
template <typename T>
// Thread-safe queue
class TsQueue {
 public:
  TsQueue() = default;
  TsQueue(const TsQueue<T>&) = delete;
  TsQueue(TsQueue<T>&&) = delete;
  TsQueue& operator=(const TsQueue<T>&) = delete;
  TsQueue& operator=(TsQueue<T>&&) = delete;
  virtual ~TsQueue() { clear(); }

  T& front() {
    std::unique_lock<std::mutex> lock(mux_);
    return queue_.front();
  }

  T& back() {
    std::unique_lock<std::mutex> lock(mux_);
    return queue_.back();
  }

  void push_front(const T& item) {
    std::unique_lock<std::mutex> lock(mux_);
    queue_.push_front(item);

    // unblock wait_until_non_empty
    std::unique_lock<std::mutex> ul(mux_blocking_);
    cv_blocking_.notify_one();
  }

  void push_front(T&& item) {
    std::unique_lock<std::mutex> lock(mux_);
    queue_.push_front(std::move(item));

    // unblock wait_until_non_empty
    std::unique_lock<std::mutex> ul(mux_blocking_);
    cv_blocking_.notify_one();
  }

  void push_back(const T& item) {
    std::unique_lock<std::mutex> lock(mux_);
    queue_.push_back(item);

    // unblock wait_until_non_empty
    std::unique_lock<std::mutex> ul(mux_blocking_);
    cv_blocking_.notify_one();
  }

  void push_back(T&& item) {
    std::unique_lock<std::mutex> lock(mux_);
    queue_.push_back(std::move(item));

    // unblock wait_until_non_empty
    std::unique_lock<std::mutex> ul(mux_blocking_);
    cv_blocking_.notify_one();
  }

  T pop_front() {
    std::unique_lock<std::mutex> lock(mux_);
    T result = std::move(queue_.front());
    queue_.pop_front();
    return result;
  }

  T pop_back() {
    std::unique_lock<std::mutex> lock(mux_);
    T result = std::move(queue_.back());
    queue_.pop_back();
    return result;
  }

  bool empty() const {
    std::unique_lock<std::mutex> lock(mux_);
    return queue_.empty();
  }

  size_t size() const {
    std::unique_lock<std::mutex> lock(mux_);
    return queue_.size();
  }

  void clear() {
    std::unique_lock<std::mutex> lock(mux_);
    queue_.clear();
  }

  void wait_until_non_empty() {
    std::unique_lock<std::mutex> ul(mux_blocking_);
    cv_blocking_.wait(ul, [this]() { return !empty(); });
  }

 protected:
  std::condition_variable cv_blocking_;
  mutable std::mutex mux_blocking_;
  mutable std::mutex mux_;
  std::deque<T> queue_;
};
}  // namespace net