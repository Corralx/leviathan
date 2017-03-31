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

#include "generator.hpp"
#include <cassert>

namespace LTL {

namespace detail {

void Generator::generate(const FormulaPtr f)
{
  FormulaPtr simplified = _simplifier.simplify(f);

  _formulas.push_back(simplified);

  if (!isa<True>(simplified) && !isa<False>(simplified))
    simplified->accept(*this);
}

void Generator::visit(const True *)
{
  assert(false && "True node found in the AST!");
}

void Generator::visit(const False *)
{
  assert(false && "False node found in the AST!");
}

void Generator::visit(const Atom *)
{
}

void Generator::visit(const Negation *t)
{
  _formulas.push_back(t->formula());
  t->formula()->accept(*this);
}

void Generator::visit(const Tomorrow *t)
{
  _formulas.push_back(t->formula());
  t->formula()->accept(*this);
}

void Generator::visit(const Yesterday *)
{
  assert(false && "Unimplemented");
}


void Generator::visit(const Always *t)
{
  _formulas.push_back(t->formula());
  _formulas.push_back(make_tomorrow(make_always(t->formula())));
  t->formula()->accept(*this);
}

void Generator::visit(const Eventually *t)
{
  _formulas.push_back(t->formula());
  _formulas.push_back(make_tomorrow(make_eventually(t->formula())));
  t->formula()->accept(*this);
}

void Generator::visit(const Conjunction *t)
{
  _formulas.push_back(t->left());
  _formulas.push_back(t->right());
  t->left()->accept(*this);
  t->right()->accept(*this);
}

void Generator::visit(const Disjunction *t)
{
  _formulas.push_back(t->left());
  _formulas.push_back(t->right());
  t->left()->accept(*this);
  t->right()->accept(*this);
}

void Generator::visit(const Then *)
{
  assert(false && "Then node found in the AST!");
}

void Generator::visit(const Iff *)
{
  assert(false && "Iff node found in the AST!");
}

void Generator::visit(const Until *t)
{
  _formulas.push_back(t->left());
  _formulas.push_back(t->right());
  _formulas.push_back(make_tomorrow(make_until(t->left(), t->right())));
  t->left()->accept(*this);
  t->right()->accept(*this);
}

void Generator::visit(const Release *r)
{
  _formulas.push_back(r->left());
  _formulas.push_back(r->right());
  _formulas.push_back(make_tomorrow(make_release(r->left(), r->right())));
  r->left()->accept(*this);
  r->right()->accept(*this);
}

void Generator::visit(const Since *)
{
  assert(false && "Unimplemented");
}

void Generator::visit(const Triggered *)
{
  assert(false && "Unimplemented");
}

void Generator::visit(const Past *)
{
  assert(false && "Unimplemented");
}

void Generator::visit(const Historically *)
{
  assert(false && "Unimplemented");
}

}
}
