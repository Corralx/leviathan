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

#include <string>
#include <memory>
#include <cstdint>

namespace LTL {

namespace detail {

class Visitor;

class Formula {
public:
  enum class Type : uint8_t {
    True,
    False,
    Atom,
    Negation,
    Tomorrow,
    Yesterday,
    Always,
    Eventually,
    Past,
    Historically,
    Conjunction,
    Disjunction,
    Then,
    Iff,
    Until,
    Release,
    Since,
    Triggered
  };

  Formula() = delete;
  Formula(Type type) : _type(type) {}
  virtual ~Formula();
  Type type() const { return _type; }
  template <typename T>
  bool isa() const
  {
    return T::type == _type;
  }

  virtual void accept(class Visitor &v) const = 0;

private:
  Type _type;
};

using FormulaPtr = std::shared_ptr<Formula>;

class True : public Formula {
public:
  True() : Formula(Type::True) {}
  virtual ~True() override {}
  static const Type type = Type::True;

  void accept(Visitor &v) const override;
};

using TruePtr = std::shared_ptr<True>;

TruePtr make_true();

class False : public Formula {
public:
  False() : Formula(Type::False) {}
  virtual ~False() override {}
  static const Type type = Type::False;

  void accept(Visitor &v) const override;
};

using FalsePtr = std::shared_ptr<False>;

FalsePtr make_false();

class Atom : public Formula {
public:
  Atom() = delete;
  Atom(const std::string &name) : Formula(Type::Atom), _name(name) {}
  virtual ~Atom() override {}
  const std::string &name() const { return _name; }
  static const Type type = Type::Atom;

  void accept(Visitor &v) const override;

private:
  std::string _name;
};

using AtomPtr = std::shared_ptr<Atom>;

AtomPtr make_atom(const std::string &name);

#define DECLARE_UNARY(_Type, _make)                             \
  class _Type : public Formula {                                \
  public:                                                       \
    _Type() = delete;                                           \
    _Type(const FormulaPtr &f) : Formula(Type::_Type), _f(f) {} \
    virtual ~_Type() override {}                                \
    const FormulaPtr &formula() const { return _f; }            \
    static const Type type = Type::_Type;                       \
                                                                \
    void accept(Visitor &v) const override;                     \
                                                                \
  private:                                                      \
    FormulaPtr _f;                                              \
  };                                                            \
  using _Type##Ptr = std::shared_ptr<_Type>;                    \
  FormulaPtr make_##_make(const FormulaPtr &f);

#define DECLARE_BINARY(_Type, _make)                  \
  class _Type : public Formula {                      \
  public:                                             \
    _Type() = delete;                                 \
    _Type(const FormulaPtr &f1, const FormulaPtr &f2) \
      : Formula(Type::_Type), _f1(f1), _f2(f2)        \
    {                                                 \
    }                                                 \
    virtual ~_Type() override {}                      \
    const FormulaPtr &left() const { return _f1; }    \
    const FormulaPtr &right() const { return _f2; }   \
    static const Type type = Type::_Type;             \
                                                      \
    void accept(Visitor &v) const override;           \
                                                      \
  private:                                            \
    FormulaPtr _f1;                                   \
    FormulaPtr _f2;                                   \
  };                                                  \
  using _Type##Ptr = std::shared_ptr<_Type>;          \
  FormulaPtr make_##_make(const FormulaPtr &f1, const FormulaPtr &f2);

DECLARE_UNARY(Negation, negation)
DECLARE_UNARY(Tomorrow, tomorrow)
DECLARE_UNARY(Yesterday, yesterday)
DECLARE_UNARY(Always, always)
DECLARE_UNARY(Eventually, eventually)
DECLARE_UNARY(Past, past)
DECLARE_UNARY(Historically, historically)

DECLARE_BINARY(Conjunction, conjunction)
DECLARE_BINARY(Disjunction, disjunction)
DECLARE_BINARY(Then, then)
DECLARE_BINARY(Iff, iff)
DECLARE_BINARY(Until, until)
DECLARE_BINARY(Release, release)
DECLARE_BINARY(Since, since)
DECLARE_BINARY(Triggered, triggered)

#undef DECLARE_UNARY
#undef DECLARE_BINARY

template <typename T>
inline bool isa(const FormulaPtr& f)
{
  return T::type == f->type();
}

template <typename T, typename ReturnT = typename std::add_const<T>::type>
inline auto fast_cast(const FormulaPtr& ptr)
  -> decltype(ptr->type(), static_cast<ReturnT *>(nullptr))
{
  if (T::type != ptr->type())
    return nullptr;

  return static_cast<ReturnT *>(ptr.get());
}

bool operator==(const FormulaPtr& f1, const FormulaPtr& f2);
bool operator!=(const FormulaPtr& f1, const FormulaPtr& f2);
}
}
