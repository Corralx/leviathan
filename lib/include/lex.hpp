/*
 Copyright (c) 2014, Nicola Gigante
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

#ifndef LEX_H_
#define LEX_H_

#include <istream>
#include <ostream>
#include <cassert>
#include <cctype>

#include <boost/optional.hpp>

namespace LTL {
namespace detail {

struct Token {
  enum Type {
    // Punctuation and atoms
    Atom = 0,
    LParen,
    RParen,

    // Binary operators and connectives
    And,
    Or,
    Implies,
    Iff,
    Until,
    Release,
    Since,
    Triggered,

    // Unary operators and connectives
    Not,
    Tomorrow,
    Always,
    Eventually,
    Yesterday,
    Past,
    Historically,
  };

  static const int NumberOfTokenTypes = Historically + 1;
  static const int FirstBinaryOp = And;
  static const int LastBinaryOp = Triggered;
  static const int FirstUnaryOp = Not;
  static const int LastUnaryOp = Historically;


  Token(Type t) : type(t) {}
  Token(std::string a) : type(Type::Atom), atom(std::move(a)) {}

  bool isAtom() const { return type == Atom; }

  bool isLParen() const { return type == LParen; }

  bool isRParen() const { return type == RParen; }

  bool isBinOp() const { return type >= FirstBinaryOp && type <= LastBinaryOp; }

  bool isUnaryOp() const { return type >= FirstUnaryOp && type <= LastUnaryOp; }

  int binOpPrecedence() const {
    // Attention: this must remain in sync with Token::Type
    constexpr int binops[] = {
      -1, -1, -1, // Atom, LParen, RParen
      30, // And
      20, // Or
      40, // Implies
      40, // Iff
      50, // Until
      50, // Release
      50, // Since
      50, // Triggered
      -1, -1, -1, -1, -1, -1, -1 // All other unary ops
    };

    return binops[type];
  }

  Type type;
  boost::optional<std::string> atom = boost::none;
};

class Lexer
{
public:
  explicit Lexer(std::istream &stream) : _stream(stream) {}
  boost::optional<Token> get() { return _token = _lex(); }
  boost::optional<Token> peek() const { return _token; }

private:
  boost::optional<Token> _lex();

  boost::optional<Token> _token = boost::none;
  std::istream &_stream;
};

std::ostream &operator<<(std::ostream &s, Token const &t);
}

using detail::Lexer;
using detail::Token;
}

#endif // LEX_H_
