#ifndef TOOLS_HPP_
#define TOOLS_HPP_
#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
template <typename Func, typename Object, typename... Args>
  requires std::is_void_v<
      std::invoke_result_t<Func, Object *, Args...>>  // 限定返回类型为 void
void SafeCall(Object *obj, Func &&func, Args &&...args) {
  if (obj) {                                    // 检查对象指针是否为空
    (obj->*func)(std::forward<Args>(args)...);  // 调用成员函数
  } else {
    return;
  }
}

class IdAllocator {
  std::atomic_uint32_t _current_id = 0;

 public:
  uint32_t AllocateId() {
    return _current_id.fetch_add(1, std::memory_order_relaxed);
  }
  void Reset() { _current_id.store(0, std::memory_order_relaxed); }
};

class NoCopyable {
 public:
  NoCopyable() = default;
  NoCopyable(const NoCopyable &) = delete;
  NoCopyable &operator=(const NoCopyable &) = delete;
  NoCopyable(NoCopyable &&) = default;
  NoCopyable &operator=(NoCopyable &&) = default;
  virtual ~NoCopyable() = default;
};

template <typename T, typename... Args>
void WaifuUnused(const T &, const Args &...) {}

template <typename T = const char *>
const char *WaifuTr(const char *value) {
  return value;
}

template <typename ValueType>
class Property {
 public:
  explicit Property(ValueType value,
                    std::function<void(ValueType)> setter = nullptr,
                    std::function<ValueType()> getter = nullptr)
      : _value(value), _setter(setter), _getter(getter) {}
  explicit Property() = default;
  Property<ValueType> &operator=(ValueType value) {
    if (_setter) {
      _setter(value);
    } else {
      _value = value;
    }
    return *this;
  }
  Property<ValueType> &operator=(const Property<ValueType> &other) = default;

  ValueType operator()() const { return Get(); }

  void Set(ValueType value) {
    if (_setter) {
      _setter(value);
    } else {
      _value = value;
    }
  }

  ValueType Get() const {
    if (_getter) {
      return _getter();
    }
    return _value;
  }

 private:
  ValueType _value;
  std::function<void(ValueType)> _setter = nullptr;
  std::function<ValueType()> _getter = nullptr;
};

template <typename Value>
class LazyVector {
  std::function<Value(int index)> _load_func;
  size_t _size = 0;

 public:

  LazyVector(std::function<Value(int index)> load_func, int size)
      : _load_func(load_func), _size(size) {}
  LazyVector() {
    _load_func = [](int) {};
  }

  explicit LazyVector(const std::vector<Value> &vector) {
    _load_func = [vector](int index) { return vector[index]; };
    _size = vector.size();
  }
  size_t size() const { return _size; }
  // support enhance for
  class Iterator {
   public:
    Iterator(LazyVector *vec, const int index) : _vec(vec), _index(index) {}
    Value operator*() const { return _vec->_load_func(_index); }
    Iterator &operator++() {
      ++_index;
      return *this;
    }
    bool operator!=(const Iterator &other) const {
      return _index != other._index;
    }

   private:
    LazyVector *_vec;
    int _index;
  };

  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, _size); }
};

#endif  // TOOLS_HPP_
