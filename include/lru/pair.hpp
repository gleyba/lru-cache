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

#ifndef LRU_PAIR_HPP
#define LRU_PAIR_HPP

#include <algorithm>
#include <type_traits>
#include <utility>

namespace LRU {

/// A pair of references to the key and value of an entry in a cache.
///
/// Instances of this class are usually the result of dereferencing an iterator.
template <typename Key, typename Value>
struct Pair final {
  using KeyType = Key;
  using ValueType = Value;
  using first_type = Key;
  using second_type = Value;

  /// Constructor.
  ///
  /// \param key The key of the pair.
  /// \param value The value of the pair.
  Pair(const Key& key, Value& value) : first(key), second(value) {
  }

  /// Generalized copy constructor.
  ///
  /// Mainly for conversion from non-const values to const values.
  ///
  /// \param other The pair to construct from.
  template <typename AnyKey,
            typename AnyValue,
            typename =
                std::enable_if_t<(std::is_convertible<AnyKey, Key>::value &&
                                  std::is_convertible<AnyValue, Value>::value)>>
  Pair(const Pair<AnyKey, AnyValue>& other)
  : first(other.first), second(other.second) {
  }

  /// Compares two pairs for equality.
  ///
  /// \param first The first pair to compare.
  /// \param second The second pair to compare.
  /// \returns True if the firest pair equals the second, else false.
  template <typename OtherPair, typename = typename OtherPair::first_type>
  friend bool operator==(const Pair& first, const OtherPair& second) noexcept {
    return first.first == second.first && first.second == second.second;
  }

  /// Compares two pairs for equality.
  ///
  /// \param first The first pair to compare.
  /// \param second The second pair to compare.
  /// \returns True if the first pair equals the second, else false.
  template <typename OtherPair, typename = typename OtherPair::first_type>
  friend bool operator==(const OtherPair& first, const Pair& second) noexcept {
    return second == first;
  }

  /// Compares two pairs for inequality.
  ///
  /// \param first The first pair to compare.
  /// \param second The second pair to compare.
  /// \returns True if the first pair does not equal the second, else false.
  template <typename OtherPair, typename = typename OtherPair::first_type>
  friend bool operator!=(const Pair& first, const OtherPair& second) noexcept {
    return !(first == second);
  }

  /// Compares two pairs for inequality.
  ///
  /// \param first The first pair to compare.
  /// \param second The second pair to compare.
  /// \returns True if the first pair does not equal the second, else false.
  template <typename OtherPair, typename = typename OtherPair::first_type>
  friend bool operator!=(const OtherPair& first, const Pair& second) noexcept {
    return second != first;
  }

  /// \returns A `std::pair` instance with the key and value of this pair.
  operator std::pair<const Key&, Value&>() noexcept {
    return {first, second};
  }

  /// \returns The key of the pair (`first`).
  const Key& key() const noexcept {
    return first;
  }

  /// \returns The value of the pair (`second`).
  Value& value() noexcept {
    return second;
  }

  /// \returns The value of the pair (`second`).
  const Value& value() const noexcept {
    return second;
  }

  /// The key of the pair.
  const Key& first;

  /// The value of the pair.
  Value& second;
};
}  // namespace LRU


#endif  // LRU_PAIR_HPP
