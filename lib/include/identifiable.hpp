/*
  Copyright (c) 2014, Matteo Bertello
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * The names of its contributors may not be used to endorse or promote
    products derived from this software without specific prior written
    permission.
*/

#pragma once

#include <cstdint>
#include <limits>
#include <ostream>

namespace LTL {
namespace detail {

template <typename Derived, typename T = size_t>
class Identifiable {
public:
  constexpr Identifiable() : _id(0) {}
  constexpr explicit Identifiable(uint64_t id) : _id(id) {}
  constexpr Identifiable(const Derived &d) : _id(d._id) {}
  inline Derived &operator=(const Derived &d)
  {
    _id = d._id;
    return *this;
  }

  inline constexpr operator T() const { return static_cast<T>(_id); }

  inline constexpr friend bool operator==(const Derived &d1, const Derived &d2)
  {
    return d1._id == d2._id;
  }

  inline constexpr friend bool operator!=(const Derived &d1, const Derived &d2)
  {
    return d1._id != d2._id;
  }

  inline Derived &operator++()
  {
    ++_id;
    return *this;
  }

  inline Derived operator++(int)
  {
    Derived temp(*this);
    ++_id;
    return temp;
  }

  inline Derived &operator--()
  {
    --_id;
    return *this;
  }

  inline Derived operator--(int)
  {
    Derived temp(*this);
    --_id;
    return temp;
  }

  inline constexpr friend bool operator<(const Derived &d1, const Derived &d2)
  {
    return d1._id < d2._id;
  }

  inline constexpr friend bool operator>(const Derived &d1, const Derived &d2)
  {
    return d1._id > d2._id;
  }

  inline constexpr friend bool operator<=(const Derived &d1, const Derived &d2)
  {
    return d1._id <= d2._id;
  }

  inline constexpr friend bool operator>=(const Derived &d1, const Derived &d2)
  {
    return d1._id >= d2._id;
  }

  template <typename U>
  inline constexpr friend Derived operator+(const Derived &d, U c)
  {
    return Derived(d._id + static_cast<T>(c));
  }

  template <typename U>
  inline constexpr friend Derived operator-(const Derived &d, U c)
  {
    return Derived(d._id - static_cast<T>(c));
  }

  inline static constexpr Derived max()
  {
    return Derived(std::numeric_limits<T>::max());
  }

  inline static constexpr Derived min()
  {
    return Derived(std::numeric_limits<T>::max());
  }

private:
  size_t _id;
};

class FrameID : public Identifiable<FrameID, int64_t> {
public:
  constexpr FrameID() {}
  constexpr explicit FrameID(int64_t id) : Identifiable(id) {}
  constexpr FrameID(const FrameID &id) : Identifiable(id) {}
  FrameID &operator=(const FrameID &) = default;
};

class FormulaID : public Identifiable<FormulaID> {
public:
  constexpr FormulaID() {}
  constexpr explicit FormulaID(uint64_t id) : Identifiable(id) {}
  constexpr FormulaID(const FormulaID &id) : Identifiable(id) {}
  FormulaID &operator=(const FormulaID &) = default;
};
}
}

template <typename T>
std::ostream &operator<<(std::ostream &os, LTL::detail::Identifiable<T> id)
{
  return os << static_cast<T>(id);
}

namespace std {
template <>
struct hash<LTL::detail::FormulaID> {
  size_t operator()(LTL::detail::FormulaID id) const
  {
    return hash<uint64_t>()(static_cast<uint64_t>(id));
  }
};
}
