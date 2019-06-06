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

#include "format.hpp"
#include "visitor.hpp"

#include <sstream>

namespace LTL {
namespace detail {

class Visitor;

class PrettyPrinter : public Visitor {
public:
  PrettyPrinter() = default;
  virtual ~PrettyPrinter() override = default;

  std::string to_string(const FormulaPtr& formula);
  std::string to_string(const Formula *formula);

protected:
  virtual void visit(const True *t) override;
  virtual void visit(const False *f) override;
  virtual void visit(const Atom *atom) override;
  virtual void visit(const Negation *negation) override;
  virtual void visit(const Tomorrow *tomorrow) override;
  virtual void visit(const Yesterday *yesterdat) override;
  virtual void visit(const Always *always) override;
  virtual void visit(const Eventually *eventually) override;
  virtual void visit(const Conjunction *conjunction) override;
  virtual void visit(const Disjunction *disjunction) override;
  virtual void visit(const Then *then) override;
  virtual void visit(const Iff *iff) override;
  virtual void visit(const Until *until) override;
  virtual void visit(const Release *release) override;
  virtual void visit(const Since *since) override;
  virtual void visit(const Triggered *triggered) override;
  virtual void visit(const Past *past) override;
  virtual void visit(const Historically *historically) override;

private:
  std::stringstream _stream;
};
}
}
