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

#include "parser.hpp"
#include "lex.hpp"

namespace LTL {
namespace detail {

boost::optional<Token> Parser::peek() {
  return _lex.peek();
}

boost::optional<Token> Parser::peek(Token::Type t, std::string const&err) {
  auto tok = peek();
  if(!tok || tok->type == t) {
    _error("Expected " + err);
    return boost::none;
  }

  return tok;
}

boost::optional<Token> Parser::consume() {
  auto tok = peek();
  if(tok)
    _lex.get();
  return tok;
}

boost::optional<Token> Parser::consume(Token::Type t, std::string const&err) {
  auto tok = peek(t, err);
  if(tok)
    _lex.get();
  return tok;
}

FormulaPtr Parser::error(std::string const&s) {
  _error(s);
  return nullptr;
}

FormulaPtr Parser::parseFormula() {
  FormulaPtr lhs = parsePrimary();
  if(!lhs)
    return nullptr;

  return parseBinaryRHS(0, std::move(lhs));
}

FormulaPtr Parser::parseBinaryRHS(int precedence, FormulaPtr lhs) {
  while(1) {
    if(!peek() || peek()->binOpPrecedence() < precedence)
       return lhs;

    Token op = *consume();

    FormulaPtr rhs = parsePrimary();
    if(!rhs)
      return nullptr;

    if(!peek() || op.binOpPrecedence() < peek()->binOpPrecedence()) {
      rhs = parseBinaryRHS(precedence + 1, std::move(rhs));
      if(!rhs)
        return nullptr;
    }

    lhs = makeBinary(op, std::move(lhs), std::move(rhs));
  }
}

FormulaPtr Parser::makeBinary(Token op, FormulaPtr lhs, FormulaPtr rhs) {
  assert(op.isBinOp());

  using FormulaMaker = FormulaPtr (*)(FormulaPtr const&, FormulaPtr const&);
  constexpr FormulaMaker makers[] = {
    0, 0, 0, // Atom, LParen, RParen
    make_conjunction, // And
    make_disjunction, // Or
    make_then, // Implies
    make_iff, // Iff
    make_until, // Until
    make_release, // Release
    make_since, // Since
    make_triggered, // Triggered
    0, 0, 0, 0, 0, 0, 0 // All other unary ops
  };

  assert(makers[op.type]);
  return makers[op.type](lhs, rhs);
}

FormulaPtr Parser::parseAtom() {
  assert(peek() && peek()->type == Token::Atom);

  auto tok = consume(); // Assume we are at an atom

  assert(tok && tok->isAtom());

  return make_atom(*tok->atom);
}

FormulaPtr Parser::parseUnary() {
  assert(peek() && peek()->isUnaryOp());

  auto tok = consume(); // consume unary op

  FormulaPtr formula = parsePrimary();
  switch(tok->type) {
    case Token::Not:
      return make_negation(formula);
    case Token::Tomorrow:
      return make_tomorrow(formula);
    case Token::Yesterday:
      return make_yesterday(formula);
    case Token::Always:
      return make_always(formula);
    case Token::Eventually:
      return make_eventually(formula);
    case Token::Past:
      return make_past(formula);
    case Token::Historically:
      return make_historically(formula);
    default:
      break;
  }

  assert(false && "Unknown unary operator!");
  return nullptr; // unreachable but MSVC complains
}

FormulaPtr Parser::parseParens() {
  assert(peek() && peek()->type == Token::LParen);

  consume(); // Consume LParen ')'

  FormulaPtr formula = parseFormula();
  if(!formula)
    return nullptr;

  if(!consume(Token::LParen, "')'"))
    return nullptr;

  return formula;
}

FormulaPtr Parser::parsePrimary() {
  if(!peek())
    return nullptr;

  if(peek()->isAtom())
    return parseAtom();
  if(peek()->isUnaryOp())
    return parseUnary();
  if(peek()->isLParen())
    return parseParens();

  return error("Expected formula");
}

} // namespace detail
} // namespace LTL
