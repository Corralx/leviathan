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

#include "boost/pool/pool_alloc.hpp"
#include "formula.hpp"
#include "identifiable.hpp"
#include "frame.hpp"
#include "model.hpp"

#include <vector>
#include <tuple>
#include <limits>
#include <stack>
#include <queue>
#include <unordered_map>
#include <random>

namespace LTL
{
namespace detail
{

using Stack = std::stack<Frame, std::deque<Frame, boost::fast_pool_allocator<Frame>>>;

class Solver
{
public:
	enum class Result : uint8_t
	{
		UNDEFINED = 0,
		SATISFIABLE,
		UNSATISFIABLE
	};

	enum class State : uint8_t
	{
		UNINITIALIZED = 0,
		INITIALIZED,
		RUNNING,
		PAUSED,
		DONE
	};

	struct Stats
	{
		uint64_t maximum_frames = 1;
		uint64_t total_frames = 1;
		uint64_t maximum_model_size = 1;
		uint64_t current_model_size = 0; // TODO: Currently not implemented
		uint64_t total_steps = 0;
		uint64_t cross_by_contradiction = 0;
		uint64_t cross_by_prune = 0;
	};

	Solver() = delete;
	~Solver() {}

	Solver(const Solver&) = delete;
	Solver(Solver&&) = delete;

	Solver& operator=(const Solver&) = delete;
	Solver& operator=(Solver&&) = delete;

	Solver(FormulaPtr formula, FrameID maximum_depth = FrameID::max());

	FormulaPtr inline Formula() const;

	inline State state() const
	{
		return _state;
	}

	inline Result satisfiability() const
	{
		return _result;
	}

	inline FrameID maximum_depth() const
	{
		return _maximum_depth;
	}

	inline const Stats& stats() const
	{
		return _stats;
	}

	Result solution();
	ModelPtr model();

private:
	FormulaPtr _formula;

	FrameID _maximum_depth;

	State _state;
	Result _result;

	struct
	{
		Bitset atom;
		Bitset negation;
		Bitset tomorrow;
    Bitset yesterday;
		Bitset always;
		Bitset eventually;
		Bitset conjunction;
		Bitset disjunction;
		Bitset until;
    Bitset release;
    Bitset since;
    Bitset triggered;
    Bitset past;
    Bitset historically;
		Bitset eventualities;

		/* This is used to do computations avoiding allocations */
		Bitset temporary;
	} _bitset;

	std::vector<FormulaID> _lhs;
	std::vector<FormulaID> _rhs;
	std::unordered_map<FormulaID, std::string> _atom_set;

	std::vector<FormulaID> _fw_eventualities_lut;
	std::vector<FormulaID> _bw_eventualities_lut;

	size_t _number_of_formulas;
	FormulaID _start_index;
	FrameID _loop_state;

	std::vector<FormulaPtr> _subformulas;
	Stack _stack;

	Stats _stats;

	bool _has_eventually;
	bool _has_until;
  bool _has_release;

	void _initialize();
	void _add_formula_for_position(const FormulaPtr& formula, FormulaID position, FormulaID lhs, FormulaID rhs);

	inline bool _check_contradiction_rule();
	inline bool _apply_conjunction_rule();
	inline bool _apply_always_rule();
	inline bool _apply_disjunction_rule();
	inline bool _apply_eventually_rule();
	inline bool _apply_until_rule();
  inline bool _apply_release_rule();

	inline void _rollback_to_latest_choice();
	inline void _update_eventualities_satisfaction();
	inline void _update_history();

	inline std::pair<bool, FrameID> _check_loop_rule() const;
	inline bool _check_prune0_rule() const;
	inline bool _check_prune_rule() const;
	inline bool _check_my_prune() const;

	void _print_stats() const;

	void __dump_frame(Frame const*frame) const;
	void __dump_frame_formulas(Frame const*frame) const;
	void __dump_requested_eventualities(Frame const*frame) const;
	void __dump_satisfied_eventualities(Frame const*frame) const;
	void __dump_current_branch() const;

};

}
}
