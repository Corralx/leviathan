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

#include "boost/dynamic_bitset.hpp"
#include "boost/pool/pool_alloc.hpp"
#include "identifiable.hpp"

#include <cstdint>

namespace LTL {
namespace detail {

using Bitset = boost::dynamic_bitset<uint64_t>;

// TODO: Remove set_ and is_ from functions name
class Eventuality {
public:
  Eventuality() : _id(-1) {}
  Eventuality(FrameID id) : _id(id) {}
  FrameID &id() { return _id; }
  FrameID id() const { return _id; }
  bool is_satisfied() const { return _id >= 0; }
  void set_satisfied(const FrameID &id) { _id = id; }
private:
  FrameID _id;
};

using Eventualities =
  std::vector<Eventuality, boost::fast_pool_allocator<Eventuality>>;

struct Frame {
  enum Type : uint8_t { UNKNOWN = 0, STEP = 1, CHOICE = 2 };

  Bitset formulas;
  Bitset to_process;
  Bitset requests; // stored here as it needs a lookup to get it from `formulas`
  Eventualities eventualities;
  FrameID id;
  FormulaID choosen_formula;
  Frame* chain;
  Frame* first;
  Frame* prev;
  Type type;

  // Builds a frame with a single formula in it (represented by the index in
  // the table) -> Start of the process
  Frame(const FrameID _id, const FormulaID _formula,
        uint64_t number_of_formulas, uint64_t number_of_eventualities)
    : formulas(number_of_formulas),
      to_process(number_of_formulas),
      requests(number_of_formulas),
      eventualities(number_of_eventualities),
      id(_id),
      choosen_formula(FormulaID::max()),
      chain(nullptr),
	  first(nullptr),
	  prev(nullptr),
      type(UNKNOWN)
  {
    formulas.set(_formula);
    to_process.set();
  }

  // Builds a frame with the same formulas of the given frame in it -> Choice
  // point
  Frame(const Frame &_frame)
    : formulas(_frame.formulas),
      to_process(_frame.to_process),
      requests(_frame.requests),
      eventualities(_frame.eventualities,
                    _frame.eventualities.get_allocator()),
      id(_frame.id),
      choosen_formula(FormulaID::max()),
      chain(_frame.chain),
	  first(nullptr),
	  prev(nullptr),
      type(UNKNOWN)
  {
  }

  // Builds a frame with the given sets of eventualities (needs to be manually
  // filled with the formulas) -> Step rule
  Frame(const FrameID _id, uint64_t number_of_formulas,
        const Eventualities &_eventualities, Frame *chainPtr)
    : formulas(number_of_formulas),
      to_process(number_of_formulas),
      requests(number_of_formulas),
      eventualities(_eventualities, _eventualities.get_allocator()),
      id(_id),
      choosen_formula(FormulaID::max()),
      chain(chainPtr),
	    first(nullptr),
	    prev(nullptr),
      type(UNKNOWN)
  {
    to_process.set();
  }
};
}
}
