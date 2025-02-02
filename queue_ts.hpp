#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

namespace ThreadSafe {

template <typename T, template <typename> class SmartPtr> class Queue {
  static_assert(std::is_same_v<SmartPtr<T>, std::unique_ptr<T>> ||
                    std::is_same_v<SmartPtr<T>, std::shared_ptr<T>>,
                "SmartPtr must be either std::unique_ptr or std::shared_ptr");

public:
  void push(const T &value);
  void push(T &&value);

  SmartPtr<T> pop_must();
  SmartPtr<T> pop_try();

  template <class Rep, class Period>
  SmartPtr<T> pop_for(const std::chrono::duration<Rep, Period> &timeout);

  template <class Clock, class Duration>
  SmartPtr<T>
  pop_until(const std::chrono::time_point<Clock, Duration> &timeout_time);

  size_t size() const;

private:
  std::queue<SmartPtr<T>> _queue;
  mutable std::mutex _lock;
  std::condition_variable _cv;
};

template <typename T, template <typename> class SmartPtr>
size_t Queue<T, SmartPtr>::size() const {
  std::lock_guard<std::mutex> lock(_lock);
  return _queue.size();
}

template <typename T, template <typename> class SmartPtr>
void Queue<T, SmartPtr>::push(const T &value) {
  std::lock_guard<std::mutex> lock(_lock);
  if constexpr (std::is_same_v<SmartPtr<T>, std::unique_ptr<T>>) {
    _queue.push(std::make_unique<T>(value));
  } else {
    _queue.push(std::make_shared<T>(value));
  }
  _cv.notify_one();
}

template <typename T, template <typename> class SmartPtr>
void Queue<T, SmartPtr>::push(T &&value) {
  std::lock_guard<std::mutex> lock(_lock);
  if constexpr (std::is_same_v<SmartPtr<T>, std::unique_ptr<T>>) {
    _queue.push(std::make_unique<T>(std::forward<T>(value)));
  } else {
    _queue.push(std::make_shared<T>(std::forward<T>(value)));
  }
  _cv.notify_one();
}

template <typename T, template <typename> class SmartPtr>
SmartPtr<T> Queue<T, SmartPtr>::pop_must() {
  std::unique_lock<std::mutex> lock(_lock);
  _cv.wait(lock, [this]() { return !_queue.empty(); });
  SmartPtr<T> value = std::move(_queue.front());
  _queue.pop();
  return value;
}

template <typename T, template <typename> class SmartPtr>
SmartPtr<T> Queue<T, SmartPtr>::pop_try() {
  std::lock_guard<std::mutex> lock(_lock);
  if (_queue.empty()) {
    return nullptr;
  }
  SmartPtr<T> value = std::move(_queue.front());
  _queue.pop();
  return value;
}

template <typename T, template <typename> class SmartPtr>
template <class Rep, class Period>
SmartPtr<T>
Queue<T, SmartPtr>::pop_for(const std::chrono::duration<Rep, Period> &timeout) {
  std::unique_lock<std::mutex> lock(_lock);
  if (_cv.wait_for(lock, timeout, [this]() { return !_queue.empty(); })) {
    SmartPtr<T> value = std::move(_queue.front());
    _queue.pop();
    return value;
  }
  return nullptr;
}

template <typename T, template <typename> class SmartPtr>
template <class Clock, class Duration>
SmartPtr<T> Queue<T, SmartPtr>::pop_until(
    const std::chrono::time_point<Clock, Duration> &timeout_time) {
  std::unique_lock<std::mutex> lock(_lock);
  if (_cv.wait_until(lock, timeout_time,
                     [this]() { return !_queue.empty(); })) {
    SmartPtr<T> value = std::move(_queue.front());
    _queue.pop();
    return value;
  }
  return nullptr;
}
}; // namespace ThreadSafe
