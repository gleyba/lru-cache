/**
* The MIT License (MIT)
* Copyright (c) 2016 Peter Goldsborough and Markus Engel
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef LRU_INTERNAL_ABSTRACT_CACHE_HPP
#define LRU_INTERNAL_ABSTRACT_CACHE_HPP

#include <cstddef>
#include <list>
#include <unordered_map>

#include "lru/internal/last-accessed.hpp"

namespace LRU {
template <typename Key,
          typename Value,
          template <typename, typename> class InformationType>
class AbstractCache {
 public:
  using size_t = std::size_t;
  using Information = InformationType<Key, Value>;
  using InformationArguments = typename Information::Arguments;

  explicit AbstractCache(size_t capacity) : _capacity(capacity) {
  }

  virtual ~AbstractCache() = default;

  virtual bool contains(const Key &key) = 0;
  virtual const Value &find(const Key &key) const = 0;

  virtual const Value &operator[](const Key &key) const {
    return find(key);
  }

  virtual const Value &insert(const Key &key, const Value &value) = 0;

  virtual void erase(const Key &key) {
    if (_last_accessed == key) {
      _erase(_last_accessed.iterator());
    }

    auto iterator = _cache.find(key);
    if (iterator != _cache.end()) {
      _erase(iterator);
    }
  }

  virtual void clear() {
    _cache.clear();
    _order.clear();
  }

  size_t size() const noexcept {
    return _cache.size();
  }

  size_t capacity() const noexcept {
    return _capacity;
  }

  size_t space_left() const noexcept {
    return _capacity - size();
  }

  bool is_empty() const noexcept {
    return size() == 0;
  }

  bool is_full() const noexcept {
    return size() == _capacity;
  }

 protected:
  using Queue = std::list<Key>;
  using QueueIterator = typename Queue::const_iterator;

  using Map = std::unordered_map<Key, Information>;
  using MapIterator = typename Map::const_iterator;

  void _erase_lru() {
    auto lru = _order.front();
    _order.pop_front();
    _cache.erase(lru);
  }

  void _erase(MapIterator iterator) {
    if (_last_accessed == iterator) {
      _last_accessed.invalidate();
    }

    _order.erase(iterator->second.order);
    _cache.erase(iterator);
  }

  Map _cache;
  Queue _order;

  mutable Internal::LastAccessed<MapIterator> _last_accessed;

  size_t _capacity;
};
}

#endif /* LRU_INTERNAL_ABSTRACT_CACHE_HPP*/