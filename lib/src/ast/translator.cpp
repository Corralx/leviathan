/*
  Copyright (c) 2017, Alex Falcon
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

#include "translator.hpp"
#include "format.hpp"
#include "pretty_printer.hpp"

namespace LTL {
namespace detail {


/*
 * Main public method available: given an LTL+P formula, it translates it to an equisatisfiable LTL formula.
 * To do so, it replaces the past operators with propositional letters and then it adds (in conjunction to the formula),
 *  some ad-hoc axioms to make the prop letter behave as the replaced operator.
 */
FormulaPtr Translator::translate(FormulaPtr f) {
  // "a" is a buffer formula, ad-hoc axioms will be added in conjunction to it
  FormulaPtr a = make_true();
  
  // "plns" is a set in which propositional letters are stored
  init_plns(plns, f);
  FormulaPtr ff = translate_p(f, a, plns);
  
  PrettyPrinter p;
  format::debug("Translator::Translated formula: ");
  format::verbose("TranslatedFormula \t=>    {}", p.to_string(ff));
  format::verbose("TranslatedFAxioms \t=>    {}", p.to_string(a));
  return conc(ff, a);
}

/* 
 * translate_p is an internal method which actually does the translation.
 * - in the base cases of True, False and Atoms, nothing is to be done
 * - in the case of future-time operators, it has only to recusively check for past-time operators 
 * - in the case of past-time operators,
 * - - first of all, recursion on the subformulae
 * - - then, a new propositional letter is created
 * - - then, "a" is modified adding the ad-hoc axioms
 * - - finally, the fresh propositional letter is returned
 */
FormulaPtr Translator::translate_p(FormulaPtr f, FormulaPtr& a, std::set<std::string>& ps) {
  PrettyPrinter p;
  FormulaPtr fp, fpl, fpr;
  if (isa<True>(f) || isa<False>(f) || isa<Atom>(f)) {
    return f;
  }
  else if (isa<Conjunction>(f)) {
    fpl = translate_p(fast_cast<Conjunction>(f)->left(), a, ps);
    fpr = translate_p(fast_cast<Conjunction>(f)->right(), a, ps);
    return make_conjunction(fpl, fpr);
  }
  else if (isa<Disjunction>(f)) {
    fpl = translate_p(fast_cast<Disjunction>(f)->left(), a, ps);
    fpr = translate_p(fast_cast<Disjunction>(f)->right(), a, ps);
    return make_disjunction(fpl, fpr);
  }
  else if (isa<Negation>(f)) {
    fp = translate_p(fast_cast<Negation>(f)->formula(), a, ps);
    return make_negation(fp);
  }
  else if (isa<Then>(f)) {
    fpl = translate_p(fast_cast<Then>(f)->left(), a, ps);
    fpr = translate_p(fast_cast<Then>(f)->right(), a, ps);
    return make_then(fpl, fpr);
  }
  else if (isa<Iff>(f)) {
    fpl = translate_p(fast_cast<Iff>(f)->left(), a, ps);
    fpr = translate_p(fast_cast<Iff>(f)->right(), a, ps);
    return make_iff(fpl, fpr);
  }
  else if (isa<Tomorrow>(f)) {
    fp = translate_p(fast_cast<Tomorrow>(f)->formula(), a, ps);
    return make_tomorrow(fp);
  }
  else if (isa<Until>(f)) {
    fpl = translate_p(fast_cast<Until>(f)->left(), a, ps);
    fpr = translate_p(fast_cast<Until>(f)->right(), a, ps);
    return make_until(fpl, fpr);
  }
  else if (isa<Release>(f)) {
    fpl = translate_p(fast_cast<Release>(f)->left(), a, ps);
    fpr = translate_p(fast_cast<Release>(f)->right(), a, ps);
    return make_release(fpl, fpr);
  }
  else if (isa<Eventually>(f)) {
    fp = translate_p(fast_cast<Eventually>(f)->formula(), a, ps);
    return make_eventually(fp);
  }
  else if (isa<Always>(f)) {
    fp = translate_p(fast_cast<Always>(f)->formula(), a, ps);
    return make_always(fp);
  }
  else if (isa<Yesterday>(f)) {
    format::debug("In Yesterday");
    fp = translate_p(fast_cast<Yesterday>(f)->formula(), a, ps);
    FormulaPtr newf = make_atom(prop_name(f, ps));
    format::debug("--- introduced propositional letter: {}", p.to_string(newf));
    a = conc(a, make_conjunction(make_negation(newf), make_always(make_iff(make_tomorrow(newf), fp)))); 
    format::verbose("{} ->    {}", p.to_string(f), p.to_string(a));
    return newf;
  }
  else if (isa<Since>(f)) {
    format::debug("In Since");
    fpl = translate_p(fast_cast<Since>(f)->left(), a, ps);
    fpr = translate_p(fast_cast<Since>(f)->right(), a, ps);
    FormulaPtr newf = make_atom(prop_name(f, ps));
    format::debug("--- introduced propositional letter: {}", p.to_string(newf));
    a = conc(a, make_conjunction(make_iff(newf, fpr), make_always(make_iff(make_tomorrow(newf), make_disjunction(make_tomorrow(fpr), make_conjunction(newf, make_tomorrow(fpl)))))));
    format::verbose("{} ->    {}", p.to_string(f), p.to_string(a));
    return newf;
  }
  else if (isa<Triggered>(f)) {
    format::debug("In Triggered");
    fpl = translate_p(fast_cast<Triggered>(f)->left(), a, ps);
    fpr = translate_p(fast_cast<Triggered>(f)->right(), a, ps);
    FormulaPtr newt = make_atom(prop_name(f, ps));
    format::debug("--- introduced propositional letter: {}", p.to_string(newt));
    FormulaPtr news = make_atom(prop_name(make_since(fpr, fpl), ps));
    format::debug("--- introduced propositional letter: {}", p.to_string(news));
    FormulaPtr newh = make_atom(prop_name(make_historically(fpr), ps));
    format::debug("--- introduced propositional letter: {}", p.to_string(newh));
    a = conc(a, 
      make_conjunction(make_conjunction(make_conjunction(make_conjunction(make_iff(newt, fpr), make_disjunction(make_conjunction(newh, fpr), make_iff(news, fpl))), make_always(make_iff(newt, make_disjunction(newh, news)))), make_always(make_iff(make_tomorrow(newh), make_conjunction(newh, make_tomorrow(fpr))))), make_always(make_iff(make_tomorrow(news), make_disjunction(make_tomorrow(fpl), make_conjunction(news, make_tomorrow(fpr))))))
    );
    format::verbose("{} ->    {}", p.to_string(f), p.to_string(a));
    return newt;
  }
  else if (isa<Past>(f)) {
    format::debug("In Past");
    fp = translate_p(fast_cast<Past>(f)->formula(), a, ps);
    FormulaPtr newf = make_atom(prop_name(f, ps));
    format::debug("--- introduced propositional letter: {}", p.to_string(newf));
    a = conc(a, make_conjunction(make_iff(newf, fp), make_always(make_iff(make_tomorrow(newf), make_disjunction(newf, fp))))); 
    format::verbose("{} ->    {}", p.to_string(f), p.to_string(a));
    return newf;
  }
  else if (isa<Historically>(f)) {
    format::debug("In Historically");
    fp = translate_p(fast_cast<Historically>(f)->formula(), a, ps);
    FormulaPtr newf = make_atom(prop_name(f, ps));
    format::debug("--- introduced propositional letter: {}", p.to_string(newf));
    a = conc(a, make_conjunction(make_conjunction(newf, fp), make_always(make_iff(make_tomorrow(newf), make_conjunction(newf, make_tomorrow(fp)))))); 
    format::verbose("{} ->    {}", p.to_string(f), p.to_string(a));
    return newf;
  }
  return f; // TODO it shouldn't come this far anyway
}

/* prop_name is used to help creating a name for the new propositional letter */
std::string Translator::prop_name (FormulaPtr f, std::set<std::string>& ps) {
  std::string tmp;
  int c = 0;
  
  tmp = prop_name_r(f, ps);
  while (ps.find(tmp) != ps.end()) {
    tmp = tmp + std::to_string(c++);
  }
  
  ps.insert(tmp);
  return tmp;
}

std::string Translator::prop_name_r (FormulaPtr f, std::set<std::string>& ps) {
  if (isa<True>(f))
    return "0";
  else if (isa<False>(f))
    return "1";
  else if (isa<Atom>(f))
    return fast_cast<Atom>(f)->name();
  else if (isa<Conjunction>(f))
    return "c" + prop_name(fast_cast<Conjunction>(f)->left(), ps);
  else if (isa<Disjunction>(f))
    return "d" + prop_name(fast_cast<Disjunction>(f)->left(), ps);
  else if (isa<Negation>(f))
    return "n" + prop_name(fast_cast<Negation>(f)->formula(), ps);
  else if (isa<Then>(f))
    return "m" + prop_name(fast_cast<Then>(f)->left(), ps);
  else if (isa<Iff>(f))
    return "i" + prop_name(fast_cast<Iff>(f)->left(), ps);
  else if (isa<Tomorrow>(f))
    return "x" + prop_name(fast_cast<Tomorrow>(f)->formula(), ps);
  else if (isa<Until>(f))
    return "u" + prop_name(fast_cast<Until>(f)->left(), ps);
  else if (isa<Release>(f))
    return "r" + prop_name(fast_cast<Release>(f)->left(), ps);
  else if (isa<Eventually>(f))
    return "f" + prop_name(fast_cast<Eventually>(f)->formula(), ps);
  else if (isa<Always>(f))
    return "g" + prop_name(fast_cast<Always>(f)->formula(), ps);
  else if (isa<Yesterday>(f))
    return "y" + prop_name(fast_cast<Yesterday>(f)->formula(), ps);
  else if (isa<Since>(f))
    return "s" + prop_name(fast_cast<Since>(f)->left(), ps);
  else if (isa<Triggered>(f))
    return "t" + prop_name(fast_cast<Triggered>(f)->left(), ps);
  else if (isa<Past>(f))
    return "p" + prop_name(fast_cast<Past>(f)->formula(), ps);
  else if (isa<Historically>(f))
    return "h" + prop_name(fast_cast<Historically>(f)->formula(), ps);  
  return "-err-";
}

/*
 * At the beginning, translate_p is called with two parameters: the formula to be translated and a "buffer", initially set to True, in which the axioms are added.
 * Everytime an axiom is added to it, conc checks whether the first formula (I always call conc(buffer, new_axiom) in such a way that buffer is the one which may be True)
 *   is True and, if it is True, it removes it.
 */
FormulaPtr Translator::conc (FormulaPtr ax, FormulaPtr fr) {
  return (isa<True>(ax) ? fr : make_conjunction(fr, ax));
}

/*
 * init_plns looks for all the already-used propositional letters in the formula and adds them in the set ps
 */
void Translator::init_plns (std::set<std::string>& ps, FormulaPtr f) {
  if (isa<True>(f) || isa<False>(f))
    return ;
  else if (isa<Atom>(f)) {
    /* only when it reaches this point, the propositional letter used is actually inserted in the set */
    ps.insert(fast_cast<Atom>(f)->name());
    //format::debug("init_plns: found prop letter, plns.size={}", plns.size());
  }
  else if (isa<Conjunction>(f)) {
    init_plns(ps, fast_cast<Conjunction>(f)->left());
    init_plns(ps, fast_cast<Conjunction>(f)->right());
  }
  else if (isa<Disjunction>(f)) {
    init_plns(ps, fast_cast<Disjunction>(f)->left());
    init_plns(ps, fast_cast<Disjunction>(f)->right());
  }
  else if (isa<Negation>(f))
    init_plns(ps, fast_cast<Negation>(f)->formula());
  else if (isa<Then>(f)) {
    init_plns(ps, fast_cast<Then>(f)->left());
    init_plns(ps, fast_cast<Then>(f)->right());
  }
  else if (isa<Iff>(f)) {
    init_plns(ps, fast_cast<Iff>(f)->left());
    init_plns(ps, fast_cast<Iff>(f)->right());
  }
  else if (isa<Tomorrow>(f))
    init_plns(ps, fast_cast<Tomorrow>(f)->formula());
  else if (isa<Until>(f)) {
    init_plns(ps, fast_cast<Until>(f)->left());
    init_plns(ps, fast_cast<Until>(f)->right());
  }
  else if (isa<Release>(f)) {
    init_plns(ps, fast_cast<Release>(f)->left());
    init_plns(ps, fast_cast<Release>(f)->right());
  }
  else if (isa<Eventually>(f))
    init_plns(ps, fast_cast<Eventually>(f)->formula());
  else if (isa<Always>(f))
    init_plns(ps, fast_cast<Always>(f)->formula());
  else if (isa<Yesterday>(f))
    init_plns(ps, fast_cast<Yesterday>(f)->formula());
  else if (isa<Since>(f)) {
    init_plns(ps, fast_cast<Since>(f)->left());
    init_plns(ps, fast_cast<Since>(f)->right());
  }
  else if (isa<Triggered>(f)) {
    init_plns(ps, fast_cast<Triggered>(f)->left());
    init_plns(ps, fast_cast<Triggered>(f)->right());
  }
  else if (isa<Past>(f))
    init_plns(ps, fast_cast<Past>(f)->formula());
  else if (isa<Historically>(f))
    init_plns(ps, fast_cast<Historically>(f)->formula());
}

}
}
