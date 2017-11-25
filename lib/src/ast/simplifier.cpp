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

#include "simplifier.hpp"

#include <cassert>

namespace LTL {
namespace detail {

FormulaPtr Simplifier::simplify(FormulaPtr formula)
{
  FormulaPtr toSimplify = formula;
  rulesApplied = true;
  while (rulesApplied) {
    rulesApplied = false;
    toSimplify->accept(*this);
    toSimplify = result;
  }

  return toSimplify;
}

void Simplifier::visit(const True *)
{
  result = make_true();
}

void Simplifier::visit(const False *)
{
  result = make_false();
}

void Simplifier::visit(const Atom *a)
{
  result = make_atom(a->name());
}

void Simplifier::visit(const Negation *n)
{
  n->formula()->accept(*this);
  if (isa<Negation>(result))  // ¬¬p ≡ p
  {
    result = fast_cast<Negation>(result)->formula();
    rulesApplied = true;
  }
  else if (isa<True>(result))  // ¬⊤ ≡ ⊥
  {
    result = make_false();
    rulesApplied = true;
  }
  else if (isa<False>(result))  // ¬⊥ ≡ ⊤
  {
    result = make_true();
    rulesApplied = true;
  }
  else if (isa<Tomorrow>(result)) {
    result =
      make_tomorrow(make_negation(fast_cast<Tomorrow>(result)->formula()));
    rulesApplied = true;
  }
  else if (isa<Eventually>(result))  // ¬◇p ≡ □¬p
  {
    result =
      make_always(make_negation(fast_cast<Eventually>(result)->formula()));
    rulesApplied = true;
  }
  else if (isa<Always>(result))  // ¬□p ≡ ◇¬p
  {
    result =
      make_eventually(make_negation(fast_cast<Always>(result)->formula()));
    rulesApplied = true;
  }
  else if (isa<Conjunction>(result)) {
    result =
      make_disjunction(make_negation(fast_cast<Conjunction>(result)->left()),
                       make_negation(fast_cast<Conjunction>(result)->right()));
    rulesApplied = true;
  }
  else if (isa<Disjunction>(result)) {
    result =
      make_conjunction(make_negation(fast_cast<Disjunction>(result)->left()),
                       make_negation(fast_cast<Disjunction>(result)->right()));
    rulesApplied = true;
  }
  else if (isa<Then>(result)) {
    result = make_conjunction(fast_cast<Then>(result)->left(),
                              make_negation(fast_cast<Then>(result)->right()));
    rulesApplied = true;
  }
  else if (isa<Iff>(result)) {
    result = make_iff(make_negation(fast_cast<Iff>(result)->left()),
                      fast_cast<Iff>(result)->right());
    rulesApplied = true;
  }
  else
    result = make_negation(result);
}

void Simplifier::visit(const Tomorrow *t)
{
  t->formula()->accept(*this);
  if (isa<True>(result))
    rulesApplied = true;
  else if (isa<False>(result)) {
    result = make_false();
    rulesApplied = true;
  }
  else if (isa<Always>(result) &&
           isa<Eventually>(fast_cast<Always>(result)->formula()))
    rulesApplied = true;
  else if (isa<Conjunction>(result) &&
           isa<Always>(fast_cast<Conjunction>(result)->right()) &&
           isa<Eventually>(
             fast_cast<Always>(fast_cast<Conjunction>(result)->right())
               ->formula())) {
    result =
      make_conjunction(make_tomorrow(fast_cast<Conjunction>(result)->left()),
                       fast_cast<Conjunction>(result)->right());
    rulesApplied = true;
  }
  else if (isa<Conjunction>(result) &&
           isa<Always>(fast_cast<Conjunction>(result)->left()) &&
           isa<Eventually>(
             fast_cast<Always>(fast_cast<Conjunction>(result)->left())
               ->formula())) {
    result =
      make_conjunction(fast_cast<Conjunction>(result)->left(),
                       make_tomorrow(fast_cast<Conjunction>(result)->right()));
    rulesApplied = true;
  }
  else if (isa<Disjunction>(result) &&
           isa<Always>(fast_cast<Disjunction>(result)->right()) &&
           isa<Eventually>(
             fast_cast<Always>(fast_cast<Disjunction>(result)->right())
               ->formula())) {
    result =
      make_disjunction(make_tomorrow(fast_cast<Disjunction>(result)->left()),
                       fast_cast<Disjunction>(result)->right());
    rulesApplied = true;
  }
  else if (isa<Disjunction>(result) &&
           isa<Always>(fast_cast<Disjunction>(result)->left()) &&
           isa<Eventually>(
             fast_cast<Always>(fast_cast<Disjunction>(result)->left())
               ->formula())) {
    result =
      make_disjunction(fast_cast<Disjunction>(result)->left(),
                       make_tomorrow(fast_cast<Disjunction>(result)->right()));
    rulesApplied = true;
  }
  else
    result = make_tomorrow(result);
}

void Simplifier::visit(const Yesterday *)
{
  assert(false && "Yesterday operator found in the formula after the translation phase");
}

void Simplifier::visit(const Always *a)
{
  a->formula()->accept(*this);
  if (isa<True>(result)) {
    result = make_true();
    rulesApplied = true;
  }
  else if (isa<False>(result)) {
    result = make_false();
    rulesApplied = true;
  }
  else if (isa<Always>(result))
    rulesApplied = true;
  else if (isa<Disjunction>(result) &&
           isa<Always>(fast_cast<Disjunction>(result)->right()) &&
           isa<Eventually>(
             fast_cast<Always>(fast_cast<Disjunction>(result)->right())
               ->formula())) {
    result =
      make_disjunction(make_always(fast_cast<Disjunction>(result)->left()),
                       fast_cast<Disjunction>(result)->right());
    rulesApplied = true;
  }
  else if (isa<Disjunction>(result) &&
           isa<Always>(fast_cast<Disjunction>(result)->left()) &&
           isa<Eventually>(
             fast_cast<Always>(fast_cast<Disjunction>(result)->left())
               ->formula())) {
    result =
      make_disjunction(fast_cast<Disjunction>(result)->left(),
                       make_always(fast_cast<Disjunction>(result)->right()));
    rulesApplied = true;
  }
  else
    result = make_always(result);
}

void Simplifier::visit(const Eventually *e)
{
  e->formula()->accept(*this);
  if (isa<True>(result)) {
    result = make_true();
    rulesApplied = true;
  }
  else if (isa<False>(result)) {
    result = make_false();
    rulesApplied = true;
  }
  else if (isa<Always>(result) &&
           isa<Eventually>(fast_cast<Always>(result)->formula()))
    rulesApplied = true;
  else if (isa<Eventually>(result))
    rulesApplied = true;
  else if (isa<Tomorrow>(result)) {
    result =
      make_tomorrow(make_eventually(fast_cast<Tomorrow>(result)->formula()));
    rulesApplied = true;
  }
  else if (isa<Conjunction>(result) &&
           isa<Always>(fast_cast<Conjunction>(result)->right()) &&
           isa<Eventually>(
             fast_cast<Always>(fast_cast<Conjunction>(result)->right())
               ->formula())) {
    result =
      make_conjunction(make_eventually(fast_cast<Conjunction>(result)->left()),
                       fast_cast<Conjunction>(result)->right());
    rulesApplied = true;
  }
  else if (isa<Conjunction>(result) &&
           isa<Always>(fast_cast<Conjunction>(result)->left()) &&
           isa<Eventually>(
             fast_cast<Always>(fast_cast<Conjunction>(result)->left())
               ->formula())) {
    result = make_conjunction(
      fast_cast<Conjunction>(result)->left(),
      make_eventually(fast_cast<Conjunction>(result)->right()));
    rulesApplied = true;
  }
  else
    result = make_eventually(result);
}

void Simplifier::visit(const Conjunction *c)
{
  c->left()->accept(*this);
  FormulaPtr left = result;
  c->right()->accept(*this);
  FormulaPtr right = result;

  if (left == right) {
    result = right;
    rulesApplied = true;
  }
  else if (isa<True>(left)) {
    result = right;
    rulesApplied = true;
  }
  else if (isa<True>(right)) {
    result = left;
    rulesApplied = true;
  }
  else if (isa<False>(left) || isa<False>(right)) {
    result = make_false();
    rulesApplied = true;
  }
  else if ((isa<Negation>(left) &&
            right == fast_cast<Negation>(left)->formula()) ||
           (isa<Negation>(right) &&
            left == fast_cast<Negation>(right)->formula())) {
    result = make_false();
    rulesApplied = true;
  }
  else if (isa<Tomorrow>(left) && isa<Tomorrow>(right)) {
    result =
      make_tomorrow(make_conjunction(fast_cast<Tomorrow>(left)->formula(),
                                     fast_cast<Tomorrow>(right)->formula()));
    rulesApplied = true;
  }
  else if (isa<Always>(left) && isa<Always>(right)) {
    result =
      make_always(make_conjunction(fast_cast<Always>(left)->formula(),
                                   fast_cast<Always>(right)->formula()));
    rulesApplied = true;
  }
  else
    result = make_conjunction(left, right);
}

void Simplifier::visit(const Disjunction *d)
{
  d->left()->accept(*this);
  FormulaPtr left = result;
  d->right()->accept(*this);
  FormulaPtr right = result;

  if (left == right) {
    result = right;
    rulesApplied = true;
  }
  else if (isa<True>(left) || isa<True>(right)) {
    result = make_true();
    rulesApplied = true;
  }
  else if (isa<False>(left)) {
    result = right;
    rulesApplied = true;
  }
  else if (isa<False>(right)) {
    result = left;
    rulesApplied = true;
  }
  else if ((isa<Negation>(left) &&
            right == fast_cast<Negation>(left)->formula()) ||
           (isa<Negation>(right) &&
            left == fast_cast<Negation>(right)->formula())) {
    result = make_true();
    rulesApplied = true;
  }
  else if (isa<Conjunction>(left))
  {
    result = make_conjunction(
      make_disjunction(fast_cast<Conjunction>(left)->left(), right),
      make_disjunction(fast_cast<Conjunction>(left)->right(), right));
    rulesApplied = true;
  }
  else if (isa<Conjunction>(right))
  {
    result = make_conjunction(
      make_disjunction(left, fast_cast<Conjunction>(right)->left()),
      make_disjunction(left, fast_cast<Conjunction>(right)->right()));
    rulesApplied = true;
  }
  else if (isa<Always>(left) &&
           isa<Eventually>(fast_cast<Always>(left)->formula()) &&
           isa<Always>(right) &&
           isa<Eventually>(fast_cast<Always>(right)->formula())) {
    result = make_always(make_eventually(make_disjunction(
      fast_cast<Eventually>(fast_cast<Always>(left)->formula())->formula(),
      fast_cast<Eventually>(fast_cast<Always>(right)->formula())->formula())));
    rulesApplied = true;
  }
  else if (isa<Tomorrow>(left) && isa<Tomorrow>(right)) {
    result =
      make_tomorrow(make_disjunction(fast_cast<Tomorrow>(left)->formula(),
                                     fast_cast<Tomorrow>(right)->formula()));
    rulesApplied = true;
  }
  else if (isa<Eventually>(left) && isa<Eventually>(right)) {
    result = make_eventually(
      make_disjunction(fast_cast<Eventually>(left)->formula(),
                       fast_cast<Eventually>(right)->formula()));
    rulesApplied = true;
  }
  else
    result = make_disjunction(left, right);
}

void Simplifier::visit(const Then *t)
{
  t->left()->accept(*this);
  FormulaPtr left = result;
  t->right()->accept(*this);
  FormulaPtr right = result;

  rulesApplied = true;
  result = make_disjunction(make_negation(left), right);
}

void Simplifier::visit(const Iff *i)
{
  i->left()->accept(*this);
  FormulaPtr left = result;
  i->right()->accept(*this);
  FormulaPtr right = result;

  rulesApplied = true;
  result = make_conjunction(make_disjunction(make_negation(left), right),
                            make_disjunction(left, make_negation(right)));
}

// TODO: !p U p =?= F p
void Simplifier::visit(const Until *u)
{
  u->left()->accept(*this);
  FormulaPtr left = result;
  u->right()->accept(*this);
  FormulaPtr right = result;

  if (left == right) {
    result = right;
    rulesApplied = true;
  }
  else if (isa<False>(right)) {
    result = make_false();
    rulesApplied = true;
  }
  else if (isa<False>(left)) {
    result = right;
    rulesApplied = true;
  }
  else if (isa<True>(left)) {
    result = make_eventually(right);
    rulesApplied = true;
  }
  else if (isa<True>(right)) {
    result = make_true();
    rulesApplied = true;
  }
  else if (isa<Tomorrow>(left) && isa<Tomorrow>(right)) {
    result = make_tomorrow(make_until(fast_cast<Tomorrow>(left)->formula(),
                                      fast_cast<Tomorrow>(right)->formula()));
    rulesApplied = true;
  }
  else if (isa<Always>(right) &&
           isa<Eventually>(fast_cast<Always>(right)->formula())) {
    result = right;
    rulesApplied = true;
  }
  else
    result = make_until(left, right);
}

void Simplifier::visit(const Release *)
{
  assert(false && "Release operator found in the formula after the translation phase");
}

void Simplifier::visit(const Since *)
{
  assert(false && "Since operator found in the formula after the translation phase");
}

void Simplifier::visit(const Triggered *)
{
  assert(false && "Triggered operator found in the formula after the translation phase");
}

void Simplifier::visit(const Past *)
{
  assert(false && "Past operator found in the formula after the translation phase");
}

void Simplifier::visit(const Historically *)
{
  assert(false && "Historically operator found in the formula after the translation phase");
}


}
}
