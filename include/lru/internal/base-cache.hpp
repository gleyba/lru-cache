/// The MIT License (MIT)
/// Copyright (c) 2016 Peter Goldsborough and Markus Engel
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to
/// deal in the Software without restriction, including without limitation the
/// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
/// sell copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
/// IN THE SOFTWARE.

#ifndef LRU_INTERNAL_BASE_CACHE_HPP
#define LRU_INTERNAL_BASE_CACHE_HPP

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <list>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "lru/insertion-result.hpp"
#include "lru/internal/base-ordered-iterator.hpp"
#include "lru/internal/base-unordered-iterator.hpp"
#include "lru/internal/definitions.hpp"
#include "lru/internal/last-accessed.hpp"
#include "lru/internal/optional.hpp"
#include "lru/internal/statistics-mutator.hpp"
#include "lru/internal/utility.hpp"
#include "lru/pair.hpp"
#include "lru/statistics.hpp"

namespace LRU {
namespace Internal {

// Macros are bad, but also more readable sometimes:
// Without this macro, it becomes a pain to have a `using` directive for every
// new member we add to the `BaseCache` and rename or remove every such
// directive when we make a change to the `BaseCache`.
// With this macro, you can simply do:
// using super = BaseCache<Key, Value, Information>;
// using BASE_CACHE_MEMBERS;
#define PUBLIC_BASE_CACHE_MEMBERS               \
  super::is_full;                               \
  using super::is_empty;                        \
  using super::clear;                           \
  using super::end;                             \
  using super::cend;                            \
  using super::operator=;                       \
  using typename super::Information;            \
  using typename super::UnorderedIterator;      \
  using typename super::UnorderedConstIterator; \
  using typename super::OrderedIterator;        \
  using typename super::OrderedConstIterator;   \
  using typename super::InitializerList;

#define PRIVATE_BASE_CACHE_MEMBERS           \
  super::_cache;                             \
  using typename super::Map;                 \
  using typename super::MapIterator;         \
  using typename super::MapConstIterator;    \
  using typename super::Queue;               \
  using typename super::QueueIterator;       \
  using super::_order;                       \
  using super::_last_accessed;               \
  using super::_capacity;                    \
  using super::_erase;                       \
  using super::_erase_lru;                   \
  using super::_move_to_front;               \
  using super::_value_from_result;           \
  using super::_key_is_last_accessed;        \
  using super::_register_miss_if_monitoring; \
  using super::_register_hit_if_monitoring;

template <typename Key,
          typename Value,
          template <typename, typename> class InformationType,
          typename HashFunction,
          typename KeyEqual>
class BaseCache {
 protected:
  using Information = InformationType<Key, Value>;
  using Queue = Internal::Queue<Key>;
  using QueueIterator = typename Queue::const_iterator;

  using Map = Internal::Map<Key, Information, HashFunction, KeyEqual>;
  using MapIterator = typename Map::iterator;
  using MapConstIterator = typename Map::const_iterator;


 public:
  using InitializerList = std::initializer_list<std::pair<Key, Value>>;
  using size_t = std::size_t;

  struct UnorderedIterator
      : public BaseUnorderedIterator<BaseCache, MapIterator> {
    using super = BaseUnorderedIterator<BaseCache, MapIterator>;
    friend BaseCache;

    UnorderedIterator() = default;

    UnorderedIterator(BaseCache& cache,
                      MapIterator iterator)  // NOLINT(runtime/explicit)
        : super(cache, iterator) {
    }
  };

  struct UnorderedConstIterator
      : public BaseUnorderedIterator<const BaseCache, MapConstIterator> {
    using super = BaseUnorderedIterator<const BaseCache, MapConstIterator>;
    friend BaseCache;

    UnorderedConstIterator() = default;

    UnorderedConstIterator(
        UnorderedIterator iterator)  // NOLINT(runtime/explicit)
        : super(*iterator._cache, iterator._iterator) {
    }

    UnorderedConstIterator(
        const BaseCache& cache,
        MapConstIterator iterator)  // NOLINT(runtime/explicit)
        : super(cache, iterator) {
    }
  };

  struct OrderedIterator : public BaseOrderedIterator<Key, Value, BaseCache> {
    using super = BaseOrderedIterator<Key, Value, BaseCache>;
    using UnderlyingIterator = typename super::UnderlyingIterator;
    friend BaseCache;

    OrderedIterator() = default;

    OrderedIterator(
        UnorderedIterator unordered_iterator)  // NOLINT(runtime/explicit)
        : super(unordered_iterator) {
    }

    OrderedIterator(BaseCache& cache, UnderlyingIterator iterator)
    : super(cache, iterator) {
    }
  };

  struct OrderedConstIterator
      : public BaseOrderedIterator<Key, const Value, const BaseCache> {
    using super = BaseOrderedIterator<Key, const Value, const BaseCache>;
    using UnderlyingIterator = typename super::UnderlyingIterator;
    friend BaseCache;

    OrderedConstIterator() = default;

    OrderedConstIterator(const UnorderedIterator&
                             unordered_iterator)  // NOLINT(runtime/explicit)
        : super(unordered_iterator) {
    }

    OrderedConstIterator(const UnorderedConstIterator&
                             unordered_iterator)  // NOLINT(runtime/explicit)
        : super(unordered_iterator) {
    }

    OrderedConstIterator(
        UnorderedIterator&& unordered_iterator)  // NOLINT(runtime/explicit)
        : super(std::move(unordered_iterator)) {
    }

    OrderedConstIterator(UnorderedConstIterator&&
                             unordered_iterator)  // NOLINT(runtime/explicit)
        : super(std::move(unordered_iterator)) {
    }

    OrderedConstIterator(
        const OrderedIterator& iterator)  // NOLINT(runtime/explicit)
        : super(iterator) {
    }

    OrderedConstIterator(
        OrderedIterator&& iterator)  // NOLINT(runtime/explicit)
        : super(std::move(iterator)) {
    }

    OrderedConstIterator(const BaseCache& cache, UnderlyingIterator iterator)
    : super(cache, iterator) {
    }
  };

  using InsertionResultType = InsertionResult<UnorderedIterator>;

  BaseCache(size_t capacity,
            const HashFunction& hash,
            const KeyEqual& key_equal)
  : _cache(0, hash, key_equal), _capacity(capacity), _last_accessed(key_equal) {
  }

  template <typename Iterator>
  BaseCache(size_t capacity,
            Iterator begin,
            Iterator end,
            const HashFunction& hash,
            const KeyEqual& key_equal)
  : BaseCache(capacity, hash, key_equal) {
    insert(begin, end);
  }

  template <typename Iterator>
  BaseCache(Iterator begin,
            Iterator end,
            const HashFunction& hash,
            const KeyEqual& key_equal)
      // This may be expensive
      : BaseCache(std::distance(begin, end), begin, end, hash, key_equal) {
  }

  template <typename Range>
  explicit BaseCache(Range&& range,
                     const HashFunction& hash,
                     const KeyEqual& key_equal)
  : BaseCache(std::begin(std::forward<Range>(range)),
              std::end(std::forward<Range>(range)),
              hash,
              key_equal) {
  }

  template <typename Range>
  BaseCache(size_t capacity,
            Range&& range,
            const HashFunction& hash,
            const KeyEqual& key_equal)
  : BaseCache(capacity, hash, key_equal) {
    insert(std::forward<Range>(range));
  }

  BaseCache(InitializerList list,
            const HashFunction& hash,
            const KeyEqual& key_equal)  // NOLINT(runtime/explicit)
      : BaseCache(list.size(), list.begin(), list.end(), hash, key_equal) {
  }

  BaseCache(size_t capacity,
            InitializerList list,
            const HashFunction& hash,
            const KeyEqual& key_equal)
  : BaseCache(capacity, list.begin(), list.end(), hash, key_equal) {
  }

  BaseCache(const BaseCache& other) = default;
  BaseCache(BaseCache&& other) = default;
  BaseCache& operator=(const BaseCache& other) = default;

  virtual ~BaseCache() = default;

  template <typename Range, typename = Internal::enable_if_range<Range>>
  BaseCache& operator=(const Range& range) {
    clear();
    for (const auto& pair : range) {
      insert(pair.first, pair.second);
    }
    return *this;
  }

  template <typename Range, typename = Internal::enable_if_range<Range>>
  BaseCache& operator=(Range&& range) {
    clear();
    for (auto& pair : range) {
      emplace(std::move(pair.first), std::move(pair.second));
    }
    return *this;
  }

  BaseCache& operator=(InitializerList list) {
    return operator=<InitializerList>(list);
  }

  virtual void swap(BaseCache& other) noexcept {
    using std::swap;

    swap(_order, other._order);
    swap(_cache, other._cache);
    swap(_last_accessed, other._last_accessed);
    swap(_capacity, other._capacity);
  }

  friend void swap(BaseCache& first, BaseCache& second) noexcept {
    first.swap(second);
  }

  bool operator==(const BaseCache& other) const noexcept {
    if (this == &other) return true;
    if (this->_cache != other._cache) return false;
    if (this->_order != other._order) return false;
    return true;
  }

  bool operator!=(const BaseCache& other) const noexcept {
    return !(*this == other);
  }

  UnorderedIterator unordered_begin() {
    return {*this, _cache.begin()};
  }

  UnorderedConstIterator unordered_begin() const {
    return unordered_cbegin();
  }

  UnorderedConstIterator unordered_cbegin() const {
    return {*this, _cache.cbegin()};
  }

  UnorderedIterator unordered_end() {
    return {*this, _cache.end()};
  }

  UnorderedConstIterator unordered_end() const {
    return unordered_cend();
  }

  UnorderedConstIterator unordered_cend() const {
    return {*this, _cache.cend()};
  }

  OrderedIterator ordered_begin() {
    return {*this, _order.begin()};
  }

  OrderedConstIterator ordered_begin() const {
    return ordered_cbegin();
  }

  OrderedConstIterator ordered_cbegin() const {
    return {*this, _order.cbegin()};
  }

  OrderedIterator ordered_end() {
    return {*this, _order.end()};
  }

  OrderedConstIterator ordered_end() const {
    return ordered_cend();
  }

  OrderedConstIterator ordered_cend() const {
    return {*this, _order.cend()};
  }

  UnorderedIterator begin() {
    return unordered_begin();
  }

  UnorderedConstIterator begin() const {
    return cbegin();
  }

  UnorderedConstIterator cbegin() const {
    return unordered_begin();
  }

  UnorderedIterator end() {
    return unordered_end();
  }

  UnorderedConstIterator end() const {
    return cend();
  }

  UnorderedConstIterator cend() const {
    return unordered_cend();
  }

  virtual bool contains(const Key& key) const {
    if (_key_is_last_accessed(key)) {
      _register_hit_if_monitoring(key);
      return true;
    }

    auto iterator = find(key);
    if (iterator != end()) {
      _last_accessed = iterator._iterator;
      return true;
    } else {
      return false;
    }
  }

  virtual const Value& lookup(const Key& key) const {
    if (key == _last_accessed) {
      _register_hit_if_monitoring(key);
      return _value_for_last_accessed();
    }

    auto iterator = find(key);
    if (iterator == end()) {
      throw LRU::Error::KeyNotFound();
    } else {
      return iterator.value();
    }
  }

  virtual Value& lookup(const Key& key) {
    if (key == _last_accessed) {
      _register_hit_if_monitoring(key);
      return _value_for_last_accessed();
    }

    auto iterator = find(key);
    if (iterator == end()) {
      throw LRU::Error::KeyNotFound();
    } else {
      return iterator.value();
    }
  }

  virtual UnorderedIterator find(const Key& key) = 0;
  virtual UnorderedConstIterator find(const Key& key) const = 0;

  virtual Value& operator[](const Key& key) {
    return lookup(key);
  }

  virtual const Value& operator[](const Key& key) const {
    return lookup(key);
  }

  virtual InsertionResultType insert(const Key& key, const Value& value) {
    auto iterator = _cache.find(key);

    // To insert, we first check if the key is already present in the cache
    // and if so, update its value and move its order iterator to the front
    // of the queue. Else, we insert the key at the end of the queue and
    // possibly pop the front if the cache has reached its capacity.

    if (iterator == _cache.end()) {
      if (is_full()) {
        _erase_lru();
      }

      auto order = _order.insert(_order.end(), key);
      auto result = _cache.emplace(key, Information(order, value));
      assert(result.second);

      _last_accessed = result.first;
      return {true, {*this, result.first}};
    } else {
      _move_to_front(iterator, value);
      return {false, {*this, iterator}};
    }
  }

  template <typename Iterator,
            typename = Internal::enable_if_iterator_over_pair<Iterator>>
  void insert(Iterator begin, Iterator end) {
    for (; begin != end; ++begin) {
      insert(begin->first, begin->second);
    }
  }

  template <typename Range, typename = Internal::enable_if_range<Range>>
  void insert(Range&& range) {
    using std::begin;
    using std::end;

    insert(begin(std::forward<Range>(range)), end(std::forward<Range>(range)));
  }

  virtual void insert(InitializerList list) {
    insert(list.begin(), list.end());
  }

  template <typename... Ks, typename... Vs>
  InsertionResultType emplace(std::piecewise_construct_t,
                              const std::tuple<Ks...>& key_arguments,
                              const std::tuple<Vs...>& value_arguments) {
    auto key = Internal::construct_from_tuple<Key>(key_arguments);
    auto iterator = _cache.find(key);

    if (iterator == _cache.end()) {
      if (is_full()) {
        _erase_lru();
      }

      auto order = _order.emplace(_order.end(), key);

      // clang-format off
        auto result = _cache.emplace(
          std::move(key),
          Information(order, value_arguments)
        );
      // clang-format on

      assert(result.second);

      _last_accessed = result.first;

      return {true, {*this, result.first}};
    } else {
      auto value = Internal::construct_from_tuple<Value>(value_arguments);
      _move_to_front(iterator, value);
      return {false, {*this, iterator}};
    }
  }

  template <typename K, typename V>
  InsertionResultType emplace(K&& key_argument, V&& value_argument) {
    auto key_tuple = std::forward_as_tuple(std::forward<K>(key_argument));
    auto value_tuple = std::forward_as_tuple(std::forward<V>(value_argument));
    return emplace(std::piecewise_construct, key_tuple, value_tuple);
  }

  virtual bool erase(const Key& key) {
    // No need to use _key_is_last_accessed here, because even
    // if it has expired, it's no problem to erase it anyway
    if (_last_accessed == key) {
      _erase(_last_accessed.key(), _last_accessed.value());
      return true;
    }

    auto iterator = _cache.find(key);
    if (iterator != _cache.end()) {
      _erase(iterator);
      return true;
    }

    return false;
  }

  virtual bool erase(UnorderedConstIterator iterator) {
    if (iterator != unordered_cend()) {
      _erase(iterator._iterator);
      return true;
    } else {
      return false;
    }
  }

  virtual bool erase(OrderedConstIterator iterator) {
    if (iterator != ordered_cend()) {
      _erase(_cache.find(iterator.key()));
      return true;
    } else {
      return false;
    }
  }

  virtual void clear() {
    _cache.clear();
    _order.clear();
    _last_accessed.invalidate();
  }

  virtual void shrink(size_t new_size) {
    if (new_size >= size()) return;
    if (new_size == 0) {
      clear();
      return;
    }

    while (size() > new_size) {
      _erase_lru();
    }
  }

  virtual size_t size() const noexcept {
    return _cache.size();
  }

  virtual void capacity(size_t new_capacity) {
    // Pop the front of the cache if we have to resize
    while (size() > new_capacity) {
      _erase_lru();
    }
    _capacity = new_capacity;
  }

  virtual size_t capacity() const noexcept {
    return _capacity;
  }

  virtual size_t space_left() const noexcept {
    return _capacity - size();
  }

  virtual bool is_empty() const noexcept {
    return size() == 0;
  }

  virtual bool is_full() const noexcept {
    return size() == _capacity;
  }

  virtual HashFunction hash_function() const {
    return _cache.hash_function();
  }

  virtual KeyEqual key_equal() const {
    return _cache.key_eq();
  }


  /// Statistics interface
  virtual void monitor(Statistics<Key>& statistics) {
    _stats = statistics;
  }

  virtual void monitor(Statistics<Key>&& statistics) {
    _stats = std::move(statistics);
  }

  template <typename... Args>
  void monitor(Args&&... args) {
    _stats = Statistics<Key>(std::forward<Args>(args)...);
  }

  virtual void stop_monitoring() {
    _stats.reset();
  }

  bool is_monitoring() const noexcept {
    return _stats.has_statistics();
  }

  virtual Statistics<Key>& statistics() {
    if (!is_monitoring()) {
      throw LRU::Error::NotMonitoring();
    }
    return _stats.get();
  }

  virtual const Statistics<Key>& statistics() const {
    if (!is_monitoring()) {
      throw LRU::Error::NotMonitoring();
    }
    return _stats.get();
  }

 protected:
  using MapInsertionResult = decltype(Map().emplace());
  using LastAccessed =
      typename Internal::LastAccessed<Key, Information, KeyEqual>;

  virtual void _move_to_front(MapIterator iterator, const Value& new_value) {
    _order.erase(iterator->second.order);

    // Insert and get the iterator (push_back returns
    // void and emplace_back returns a reference ...)
    auto new_order = _order.insert(_order.end(), iterator->first);

    iterator->second.order = new_order;
    iterator->second.value = new_value;

    _last_accessed = iterator;
  }

  virtual void _erase_lru() {
    _erase(_cache.find(_order.front()));
  }

  virtual void _erase(MapConstIterator iterator) {
    if (_last_accessed == iterator) {
      _last_accessed.invalidate();
    }

    _order.erase(iterator->second.order);
    _cache.erase(iterator);
  }

  virtual void _erase(const Key& key, const Information& information) {
    if (key == _last_accessed) {
      _last_accessed.invalidate();
    }

    _order.erase(information.order);

    // Requires an additional hash-lookup, whereas erase(iterator) doesn't
    _cache.erase(key);
  }

  virtual Value& _value_from_result(MapInsertionResult& result) {
    // `result.first` is the map iterator (to a pair), whose `second` member is
    // the information object, whose `value` member is the value stored.
    return result.first->second.value;
  }

  virtual bool _key_is_last_accessed(const Key& key) const noexcept {
    return _last_accessed == key;
  }

  virtual Value& _value_for_last_accessed() {
    return _last_accessed.value().value;
  }

  virtual const Value& _value_for_last_accessed() const {
    return _last_accessed.value().value;
  }

  virtual void _register_hit_if_monitoring(const Key& key) const {
    if (is_monitoring()) {
      _stats.register_hit(key);
    }
  }

  virtual void _register_miss_if_monitoring(const Key& key) const {
    if (is_monitoring()) {
      _stats.register_miss(key);
    }
  }

  Map _cache;
  Queue _order;

  mutable StatisticsMutator<Key> _stats;
  mutable LastAccessed _last_accessed;

  size_t _capacity;
};
}  // namespace Internal
}  // namespace LRU

#endif  // LRU_INTERNAL_BASE_CACHE_HPP
