# thread_save_queue
一个线程安全的队列/最小最大堆

# Queue
template <typename T, template<typename...> class SmartPtr, class Cmp = void>,第一个参数为数据类型，第二个参数为std::shared_ptr或std::unique_ptr,第三个参数如果没有则Queue内部实现为std::queue,如果存在则Queue内部实现为std::prority_queue，并以cmp作为比较

## push
  void push(const T &value); ->push(SmartPtr<T>(new T(value)))  
  void push(T &&value);    ->push(SmartPtr<T>(new T(std::forward<T>(value))))  
  添加到queue或prority_queue必须是智能指针，则没有栈数据失效非法访问的问题

## pop
  SmartPtr<T> pop_must(); // 阻塞直到pop完成  
  SmartPtr<T> pop_try();  // 尝试pop，失败返回nullptr  
  SmartPtr<T> pop_for(const std::chrono::duration<Rep, Period> &timeout); // 在timeout一段时间内之前尝试获取，失败返回nullptr  
  SmartPtr<T> pop_until(const std::chrono::time_point<Clock, Duration> &timeout_time); //在timeout_time时间节点之前尝试获取，失败返回nullptr

## 并发
  以mutex和condition_variable实现

## SharedQueue和UniqueQueue
  是Queue的std::shared_ptr和std::unique_ptr的快捷别名

# wrapper
 由于std::prority_queue存储智能指针，并且需要堆指针所指内容(非指针)排序，所以需要一层包装，即排序指针实际上是排序指针所指的结构，也是用户传入的比较

