// Copyright (c) by respective owners including Yahoo!, Microsoft, and
// individual contributors. All rights reserved. Released under a BSD (revised)
// license as described in the file LICENSE.

#pragma once
#define NOMINMAX
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstdint>

#ifdef _WIN32
#define __INLINE
#else
#define __INLINE inline
#endif

#ifndef VW_NOEXCEPT
#include "vw_exception.h"
#endif

#include "memory.h"

const size_t erase_point = ~((1u << 10u) - 1u);

template <class T>
struct v_array
{
  // private:
  T* _begin;
  T* _end;

 public:
  T* end_array;
  size_t erase_count;

  // private:
  void reserve_nocheck(size_t length)
  {
    if (capacity() == length)
    {
      return;
    }
    size_t old_len = size();

    T* temp = (T*)realloc(_begin, sizeof(T) * length);
    if ((temp == nullptr) && ((sizeof(T) * length) > 0))
    {
      THROW_OR_RETURN("realloc of " << length << " failed in resize().  out of memory?");
    }
    else
      _begin = temp;

    _end = _begin + std::min(old_len, length);
    end_array = _begin + length;
    memset(_end, 0, (end_array - _end) * sizeof(T));
  }

 public:
  // enable C++ 11 for loops
  inline T*& begin() { return _begin; }
  inline T*& end() { return _end; }

  inline const T* begin() const { return _begin; }
  inline const T* end() const { return _end; }

  inline const T* cbegin() const { return _begin; }
  inline const T* cend() const { return _end; }

  // v_array cannot have a user-defined constructor, because it participates in various unions.
  // union members cannot have user-defined constructors.
  // v_array() : _begin(nullptr), _end(nullptr), end_array(nullptr), erase_count(0) {}
  // ~v_array() {
  //  delete_v();
  // }

  // back and pop are not bounds checked.
  // Undefined behavior if they are called on empty v_arrays
  inline T& back() { return *(_end - 1); }
  inline const T& back() const { return *(_end - 1); }
  inline void pop() { (--_end)->~T(); }
  inline bool empty() const { return _begin == _end; }

  // insert() and remove() don't follow the standard spec, which calls for iterators
  // instead of indices. But these fn signatures follow our usage better.
  // These functions do not check bounds, undefined behavior if they are called
  // on out-of-bounds indices
  // insert before the indexed element
  inline void insert(size_t idx, const T& elem)
  {
    if (_end == end_array)
      reserve_nocheck(2 * capacity() + 3);
    _end++;
    memmove(&_begin[idx + 1], &_begin[idx], (size() - (idx + 1)) * sizeof(T));
    _begin[idx] = elem;
  }
  // erase indexed element
  inline void erase(size_t idx)
  {
    _begin[idx].~T();
    memmove(&_begin[idx], &_begin[idx + 1], (size() - (idx + 1)) * sizeof(T));
    --_end;
  }

  T& operator[](size_t i) { return _begin[i]; }
  const T& operator[](size_t i) const { return _begin[i]; }
  inline size_t size() const { return _end - _begin; }
  inline size_t capacity() const { return end_array - _begin; }

  // change the number of elements in the vector
  void resize(size_t length)
  {
    auto old_size = size();
    // if new length is smaller than current size destroy the excess elements
    for (auto idx = length; idx < old_size; ++idx)
    {
      _begin[idx].~T();
    }
    if (capacity() < length)
    {
      reserve(length);
    }
    _end = _begin + length;
    // default construct any newly added elements
    // TODO: handle non-default constructable objects
    // requires second interface
    /*
    for (auto idx = old_size; idx < length; ++idx)
    {
      new (&_begin[idx]) T();
    }
    */
  }

  void shrink_to_fit()
  {
    if (size() < capacity())
    {
      if (empty())
      {
        // realloc on size 0 doesn't have a specified behavior
        // just shrink to 1 for now (alternatively, call delete_v())
        reserve_nocheck(1);
      }
      else
      {
        reserve_nocheck(size());
      }
    }
  }
  // reserve enough space for the specified number of elements
  void reserve(size_t length)
  {
    if (capacity() < length)
      reserve_nocheck(length);
  }

  void clear()
  {
    if (++erase_count & erase_point)
    {
      shrink_to_fit();
      erase_count = 0;
    }
    for (T* item = _begin; item != _end; ++item) item->~T();
    _end = _begin;
  }
  void reset()
  {
    _begin = nullptr;
    _end = nullptr;
    end_array = nullptr;
    erase_count = 0;
  }
  void delete_v()
  {
    if (_begin != nullptr)
    {
      for (T* item = _begin; item != _end; ++item) item->~T();
      free(_begin);
    }
    reset();
  }
  void push_back(const T& new_ele)
  {
    if (_end == end_array)
      reserve_nocheck(2 * capacity() + 3);
    new (_end++) T(new_ele);
  }

  void push_back_unchecked(const T& new_ele) { new (_end++) T(new_ele); }

  template <class... Args>
  void emplace_back(Args&&... args)
  {
    if (_end == end_array)
      reserve_nocheck(2 * capacity() + 3);
    new (_end++) T(std::forward<Args>(args)...);
  }

  size_t find_sorted(const T& ele) const  // index of the smallest element >= ele, return true if element is in the
                                          // array
  {
    size_t size = _end - _begin;
    size_t a = 0;
    size_t b = size;
    size_t i = (a + b) / 2;

    while (b - a > 1)
    {
      if (_begin[i] < ele)  // if a = 0, size = 1, if in while we have b - a >= 1 the loop is infinite
        a = i;
      else if (_begin[i] > ele)
        b = i;
      else
        return i;

      i = (a + b) / 2;
    }

    if ((size == 0) || (_begin[a] > ele) || (_begin[a] == ele))  // pusta tablica, nie wchodzi w while
      return a;
    else  // size = 1, ele = 1, _begin[0] = 0
      return b;
  }
  size_t unique_add_sorted(const T& new_ele)
  {
    size_t index = 0;
    size_t size = _end - _begin;
    size_t to_move;

    if (!contain_sorted(new_ele, index))
    {
      if (_end == end_array)
        reserve_nocheck(2 * capacity() + 3);

      to_move = size - index;

      if (to_move > 0)
        memmove(_begin + index + 1, _begin + index,
            to_move * sizeof(T));  // kopiuje to_move*.. bytow z _begin+index do _begin+index+1

      _begin[index] = new_ele;

      _end++;
    }

    return index;
  }
  bool contain_sorted(const T& ele, size_t& index)
  {
    index = find_sorted(ele);

    if (index == this->size())
      return false;

    if (_begin[index] == ele)
      return true;

    return false;
  }
};

template <class T>
inline v_array<T> v_init()
{
  return {nullptr, nullptr, nullptr, 0};
}

template <class T>
void copy_array(v_array<T>& dst, const v_array<T>& src)
{
  dst.clear();
  dst.reserve(src.size());
  push_many(dst, src._begin, src.size());
}

// use to copy arrays of types with non-trivial copy constructors, such as shared_ptr
template <class T>
void copy_array_no_memcpy(v_array<T>& dst, const v_array<T>& src)
{
  dst.clear();
  dst.reserve(src.size());
  for (auto item : src) dst.push_back(item);
}

template <class T>
void copy_array(v_array<T>& dst, const v_array<T>& src, T (*copy_item)(const T&))
{
  dst.clear();
  dst.reserve(src.size());
  for (auto item : src) dst.push_back(copy_item(item));
}

template <class T>
void push_many(v_array<T>& v, const T* _begin, size_t num)
{
  if (v._end + num >= v.end_array)
    v.reserve_nocheck(std::max(2 * v.capacity() + 3, v._end - v._begin + num));
#ifdef _WIN32
  memcpy_s(v._end, v.size() - (num * sizeof(T)), _begin, num * sizeof(T));
#else
  memcpy(v._end, _begin, num * sizeof(T));
#endif
  v._end += num;
}

template <class T>
void calloc_reserve(v_array<T>& v, size_t length)
{
  v._begin = calloc_or_throw<T>(length);
  v._end = v._begin;
  v.end_array = v._begin + length;
}

template <class T>
v_array<T> pop(v_array<v_array<T> >& stack)
{
  if (stack._end != stack._begin)
    return *(--stack._end);
  else
    return v_array<T>();
}

template <class T>
bool v_array_contains(v_array<T>& A, T x)
{
  for (T* e = A._begin; e != A._end; ++e)
    if (*e == x)
      return true;
  return false;
}

template <class T>
std::ostream& operator<<(std::ostream& os, const v_array<T>& v)
{
  os << '[';
  for (T* i = v._begin; i != v._end; ++i) os << ' ' << *i;
  os << " ]";
  return os;
}

template <class T, class U>
std::ostream& operator<<(std::ostream& os, const v_array<std::pair<T, U> >& v)
{
  os << '[';
  for (std::pair<T, U>* i = v._begin; i != v._end; ++i) os << ' ' << i->first << ':' << i->second;
  os << " ]";
  return os;
}

typedef v_array<unsigned char> v_string;

inline v_string string2v_string(const std::string& s)
{
  v_string res = v_init<unsigned char>();
  if (!s.empty())
    push_many(res, (unsigned char*)s.data(), s.size());
  return res;
}

inline std::string v_string2string(const v_string& v_s)
{
  std::string res;
  for (unsigned char* i = v_s._begin; i != v_s._end; ++i) res.push_back(*i);
  return res;
}
