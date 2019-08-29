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

#include "formula.hpp"
#include <set>

namespace LTL {
namespace detail {

class Translator {
public:
  Translator() {};
  ~Translator() {};
  FormulaPtr translate(FormulaPtr f);
  
private:
  FormulaPtr translate_p(FormulaPtr f, FormulaPtr& a, std::set<std::string>& plns);
  FormulaPtr conc(FormulaPtr ax, FormulaPtr fr);
  std::string prop_name(FormulaPtr f, std::set<std::string>& plns);
  std::string prop_name_r(FormulaPtr f, std::set<std::string>& plns);
  void init_plns(std::set<std::string>& plns, FormulaPtr f);
  std::set<std::string> plns;
};

}
}
