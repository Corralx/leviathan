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

#include <map>
#include "lex.hpp"

#include <iostream>

namespace LTL {
namespace detail {

namespace {
boost::optional<Token> symbol(std::istream &s)
{
  char ch = char(s.peek());

  switch (ch) {
    case '(':
      s.get();
      return Token{Token::LParen};
    case ')':
      s.get();
      return Token{Token::RParen};
    case '!':
    case '~':
      s.get();
      return Token{Token::Not};
    // '&' or '&&'
    case '&':
      s.get();
      if (s.peek() == '&')
        s.get();
      return Token{Token::And};

    // '|' or '||'
    case '|':
      s.get();
      if (s.peek() == '|')
        s.get();
      return Token{Token::Or};

    // '->'
    case '-':
      s.get();
      if (s.peek() == '>') {
        s.get();
        return Token{Token::Implies};
      }
      return boost::none;

    // '=' or '=>'
    case '=':
      s.get();
      if (s.peek() == '>') {
        s.get();
        return Token{Token::Implies};
      }
      else {
        return Token{Token::Iff};
      }

    // '<->' or '<=>' or '<>'
    case '<':
      s.get();
      if (s.peek() == '-' || s.peek() == '=')
        ch = char(s.get());

      if (s.peek() == '>') {
        s.get();
        return ch != '<' ? Token{Token::Iff} : Token{Token::Eventually};
      }
      return boost::none;

    case '[':
      s.get();
      if (s.peek() == ']') {
        s.get();
        return Token{Token::Always};
      }
      return boost::none;
  }

  return boost::none;
}

boost::optional<Token> keyword(std::istream &s)
{
  static std::map<std::string, Token::Type> keywords = {
    {"NOT", Token::Not},      {"AND", Token::And},
    {"OR", Token::Or},        {"THEN", Token::Implies},
    {"IFF", Token::Iff},      {"X", Token::Tomorrow},
    {"U", Token::Until},      {"R", Token::Release},
    {"V", Token::Release},    {"G", Token::Always},
    {"F", Token::Eventually}, {"Y", Token::Yesterday},
    {"S", Token::Since},      {"T", Token::Triggered},
    {"P", Token::Past},       {"H", Token::Historically}};

  std::string kw;

  if (!s.good())
    return boost::none;

  if (isalpha(s.peek())) {
    kw += char(s.peek());
    s.get();
  }
  else
    return boost::none;

  while (s.good() && isalnum(s.peek())) {
    kw += char(s.peek());
    s.get();
  }
  if (!s.good() && !s.eof()) {
    std::cout << "Li mortacci tua\n";
    return boost::none;
  }

  auto it = keywords.find(kw);
  if (it == keywords.end())
    return Token{kw};
  else
    return Token{it->second};
}

}  // namespace

boost::optional<Token> Lexer::_lex()
{
  while (_stream.good() && isspace(_stream.peek())) {
    _stream.get();
  }

  if (!_stream.good())
    return boost::none;

  boost::optional<Token> t = symbol(_stream);
  if (t)
    return t;

  t = keyword(_stream);

  return t;
}

std::ostream &operator<<(std::ostream &s, Token const &t)
{
  static std::map<Token::Type, std::string> toks = {
    { Token::LParen,       "(" },
    { Token::RParen,       ")" },
    { Token::Not,          "!" },
    { Token::And,          "&" },
    { Token::Or,           "|" },
    { Token::Implies,     "->" },
    { Token::Iff,        "<->" },
    { Token::Tomorrow,     "X" },
    { Token::Until,        "U" },
    { Token::Release,      "R" },
    { Token::Always,       "G" },
    { Token::Eventually,   "F" },
    { Token::Yesterday,    "Y" },
    { Token::Since,        "S" },
    { Token::Triggered,    "T" },
    { Token::Past,         "P" },
    { Token::Historically, "H" }
  };
  
  if(t.atom)
    s << *t.atom;
  else
    s << toks[t.type];

  return s;
}

}  // namespace detail
}  // namespace LTL
