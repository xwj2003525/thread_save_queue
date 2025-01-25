#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

// 限定只有sharedptr 或者 uniqueptr合法
// usage Queue<int,shared_ptr> q;

namespace ThreadSafe {

namespace CmpWrapper {
  template <typename T, typename Cmp>
  struct SmartPtrCmp{
    SmartPtrCmp(const Cmp &cmp = Cmp{}):c(cmp){}
    Cmp c;

    template <typename SmartPtr>
    bool operator()(const SmartPtr &a, const SmartPtr &b) const {
        return c(*a, *b); 
    }
  };
};

template <typename T, template<typename...> class SmartPtr, class Cmp=void>
class Queue {

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

protected:
  using Wrapper = CmpWrapper::SmartPtrCmp<T, Cmp>;
  using Container = std::conditional_t<
      std::is_void_v<Cmp>, std::queue<SmartPtr<T>>,
      std::priority_queue<SmartPtr<T>, std::vector<SmartPtr<T>>, Wrapper>>;

  SmartPtr<T> _pop();
  Container _container;
  std::mutex _lock;
  std::condition_variable _cv;
};

template <typename T, template <typename...> class SmartPtr, typename Comparator>
void Queue<T, SmartPtr, Comparator>::push(const T &value) {
  std::lock_guard<std::mutex> lock(_lock);
  _container.push(SmartPtr<T>(new T(value)));
  _cv.notify_one();
}

template <typename T, template <typename...> class SmartPtr, typename Comparator>
void Queue<T, SmartPtr, Comparator>::push(T &&value) {
  std::lock_guard<std::mutex> lock(_lock);
  _container.push(SmartPtr<T>(new T(std::forward<T>(value))));
  _cv.notify_one();
}

template <typename T, template <typename...> class SmartPtr, typename Comparator>
SmartPtr<T> Queue<T, SmartPtr, Comparator>::_pop() {
  SmartPtr<T> ret;

  if constexpr (std::is_same_v<decltype(_container), std::queue<SmartPtr<T>>>) {
    ret = std::move(_container.front());
  } else {
    ret = std::move(const_cast<SmartPtr<T>&>(_container.top()));
  }

  _container.pop();

  return ret;
}

// 实现 pop_must 方法
template <typename T, template <typename...> class SmartPtr, typename Comparator>
SmartPtr<T> Queue<T, SmartPtr, Comparator>::pop_must() {
  std::unique_lock<std::mutex> lock(_lock);
  if (_container.empty()) {
    _cv.wait(lock, [this]() { return !_container.empty(); });
  }
  return _pop();
}

// 实现 pop_try 方法
template <typename T, template <typename...> class SmartPtr, typename Comparator>
SmartPtr<T> Queue<T, SmartPtr, Comparator>::pop_try() {
  std::lock_guard<std::mutex> lock(_lock);
  if (_container.empty()) {
    return nullptr;
  }

  return _pop();
}

// 实现 pop_for 方法
template <typename T, template <typename...> class SmartPtr, typename Comparator>
template <class Rep, class Period>
SmartPtr<T> Queue<T, SmartPtr, Comparator>::pop_for(
    const std::chrono::duration<Rep, Period> &timeout) {
  std::unique_lock<std::mutex> lock(_lock);
  if (_cv.wait_for(lock, timeout, [this]() { return !_container.empty(); })) {
    return _pop();
  }
  return nullptr;
}

// 实现 pop_until 方法
template <typename T, template <typename...> class SmartPtr, typename Comparator>
template <class Clock, class Duration>
SmartPtr<T> Queue<T, SmartPtr, Comparator>::pop_until(
    const std::chrono::time_point<Clock, Duration> &timeout_time) {
  std::unique_lock<std::mutex> lock(_lock);
  if (_cv.wait_until(lock, timeout_time,
                     [this]() { return !_container.empty(); })) {

    return _pop();
  }
  return nullptr;
}

template <typename T, class Cmp = void>
using SharedQueue = Queue<T, std::shared_ptr, Cmp>;

template <typename T, class Cmp = void>
using UniqueQueue = Queue<T, std::unique_ptr, Cmp>;
}; // namespace ThreadSafe
