#include "solver.hpp"

#include "ast/generator.hpp"
#include "utility.hpp"
#include "pretty_printer.hpp"
#include "format.hpp"
#include "ast/clause_counter.hpp"

#include <stack>
#include <iostream>
#include <cassert>
#include <csignal>
#include <atomic>
#include "std14/memory"

#ifdef _MSC_VER
	#define __builtin_expect(cond, value) (cond)
#endif

namespace LTL
{

namespace detail
{

static void signal_handler(int /*signal*/);

namespace colors = format::colors;

Solver::Solver(FormulaPtr formula, FrameID maximum_depth, bool use_sat)
	: _formula(formula), _maximum_depth(maximum_depth), _use_sat_solver(use_sat), _state(State::UNINITIALIZED), _result(Result::UNDEFINED), _start_index(0),
	_loop_state(0), _has_eventually(false), _has_until(false), _has_not_until(false)
{
	_initialize();
}

void Solver::_initialize()
{
	format::debug("\nInitializing solver...");
	_atom_set.clear();

	format::debug("Simplifing formula...");
	Simplifier simplifier;
	_formula = simplifier.simplify(_formula);

	format::debug("Generating subformulas...");
	Generator gen;
	gen.generate(_formula);
	_subformulas = gen.formulas();

	if (_subformulas.size() == 1)
	{
		if (isa<True>(_subformulas[0]))
		{
			_result = Result::SATISFIABLE;
			_state = State::DONE;
			return;
		}
		else if (isa<False>(_subformulas[0]))
		{
			_result = Result::UNSATISFIABLE;
			_state = State::DONE;
			return;
		}
	}

	std::function<bool(const FormulaPtr&, const FormulaPtr&)> compareFunc = [&compareFunc](const FormulaPtr& a, const FormulaPtr& b)
	{
		if (isa<Atom>(a) && isa<Atom>(b))
			return std::lexicographical_compare(fast_cast<Atom>(a)->name().begin(), fast_cast<Atom>(a)->name().end(),
												fast_cast<Atom>(b)->name().begin(), fast_cast<Atom>(b)->name().end());

		if (isa<Negation>(a) && isa<Negation>(b))
			return compareFunc(fast_cast<Negation>(a)->formula(), fast_cast<Negation>(b)->formula());

		if (isa<Negation>(a))
		{
			if (fast_cast<Negation>(a)->formula() == b)
				return false;

			return compareFunc(fast_cast<Negation>(a)->formula(), b);
		}

		if (isa<Negation>(b))
		{
			if (fast_cast<Negation>(b)->formula() == a)
				return true;

			return compareFunc(a, fast_cast<Negation>(b)->formula());
		}

		if (isa<Tomorrow>(a) && isa<Tomorrow>(b))
			return compareFunc(fast_cast<Tomorrow>(a)->formula(), fast_cast<Tomorrow>(b)->formula());

		if (isa<Tomorrow>(a))
		{
			if (fast_cast<Tomorrow>(a)->formula() == b)
				return false;

			return compareFunc(fast_cast<Tomorrow>(a)->formula(), b);
		}

		if (isa<Tomorrow>(b))
		{
			if (fast_cast<Tomorrow>(b)->formula() == a)
				return true;

			return compareFunc(a, fast_cast<Tomorrow>(b)->formula());
		}

		if (isa<Always>(a) && isa<Always>(b))
			return compareFunc(fast_cast<Always>(a)->formula(), fast_cast<Always>(b)->formula());

		if (isa<Eventually>(a) && isa<Eventually>(b))
			return compareFunc(fast_cast<Eventually>(a)->formula(), fast_cast<Eventually>(b)->formula());

		if (isa<Conjunction>(a) && isa<Conjunction>(b))
		{
			if (fast_cast<Conjunction>(a)->left() != fast_cast<Conjunction>(b)->left())
				return compareFunc(fast_cast<Conjunction>(a)->left(), fast_cast<Conjunction>(b)->left());
			else
				return compareFunc(fast_cast<Conjunction>(a)->right(), fast_cast<Conjunction>(b)->right());
		}

		if (isa<Disjunction>(a) && isa<Disjunction>(b))
		{
			if (fast_cast<Disjunction>(a)->left() != fast_cast<Disjunction>(b)->left())
				return compareFunc(fast_cast<Disjunction>(a)->left(), fast_cast<Disjunction>(b)->left());
			else
				return compareFunc(fast_cast<Disjunction>(a)->right(), fast_cast<Disjunction>(b)->right());
		}

		if (isa<Until>(a) && isa<Until>(b))
		{
			if (fast_cast<Until>(a)->left() != fast_cast<Until>(b)->left())
				return compareFunc(fast_cast<Until>(a)->left(), fast_cast<Until>(b)->left());
			else
				return compareFunc(fast_cast<Until>(a)->right(), fast_cast<Until>(b)->right());
		}

		if (isa<Then>(a) || isa<Then>(b))
			assert(false);

		if (isa<Iff>(a) && isa<Iff>(b))
			assert(false);

		return a->type() < b->type();
	};

	std::sort(_subformulas.begin(), _subformulas.end(), compareFunc);

	auto last = std::unique(_subformulas.begin(), _subformulas.end());
	_subformulas.erase(last, _subformulas.end());

	format::debug("Found {} subformulas", _subformulas.size());
	format::debug("Building data structure...");

	FormulaID current_index(0);

	_number_of_formulas = _subformulas.size();
	_bitset.atom.resize(_number_of_formulas);
	_bitset.negation.resize(_number_of_formulas);
	_bitset.tomorrow.resize(_number_of_formulas);
	_bitset.always.resize(_number_of_formulas);
	_bitset.eventually.resize(_number_of_formulas);
	_bitset.conjunction.resize(_number_of_formulas);
	_bitset.disjunction.resize(_number_of_formulas);
	_bitset.until.resize(_number_of_formulas);
	_bitset.not_until.resize(_number_of_formulas);
	_bitset.temporary.resize(_number_of_formulas);

	_lhs = std::vector<FormulaID>(_number_of_formulas, FormulaID::max());
	_rhs = std::vector<FormulaID>(_number_of_formulas, FormulaID::max());

	for (const auto& f : _subformulas)
	{
		if (f == _formula)
			_start_index = current_index;

		FormulaID lhs(0), rhs(0);
		FormulaPtr left = nullptr, right = nullptr;

		if (isa<Negation>(f))
		{
			if (isa<Until>(fast_cast<Negation>(f)->formula()))
			{
				left = simplifier.simplify(make_negation(fast_cast<Until>(fast_cast<Negation>(f)->formula())->left()));
				right = simplifier.simplify(make_negation(fast_cast<Until>(fast_cast<Negation>(f)->formula())->right()));
			}
			else
				left = fast_cast<Negation>(f)->formula();
		}
		else if (isa<Tomorrow>(f))
			left = fast_cast<Tomorrow>(f)->formula();
		else if (isa<Always>(f))
			left = fast_cast<Always>(f)->formula();
		else if (isa<Eventually>(f))
			left = fast_cast<Eventually>(f)->formula();
		else if (isa<Conjunction>(f))
		{
			left = fast_cast<Conjunction>(f)->left();
			right = fast_cast<Conjunction>(f)->right();
		}
		else if (isa<Disjunction>(f))
		{
			left = fast_cast<Disjunction>(f)->left();
			right = fast_cast<Disjunction>(f)->right();
		}
		else if (isa<Until>(f))
		{
			left = fast_cast<Until>(f)->left();
			right = fast_cast<Until>(f)->right();
		}
		else if (isa<Then>(f))
			assert(false);
		else if (isa<Iff>(f))
			assert(false);

		if (left)
			lhs = FormulaID(static_cast<uint64_t>(std::lower_bound(_subformulas.begin(), _subformulas.end(), left, compareFunc) - _subformulas.begin()));
		if (right)
			rhs = FormulaID(static_cast<uint64_t>(std::lower_bound(_subformulas.begin(), _subformulas.end(), right, compareFunc) - _subformulas.begin()));

		_add_formula_for_position(f, current_index++, lhs, rhs);
	}

	format::debug("Generating eventualities...");
	_fw_eventualities_lut = std::vector<FormulaID>(_number_of_formulas, FormulaID::max());
	std::vector<FormulaPtr> eventualities;
	for (uint64_t i = 0; i < _subformulas.size(); ++i)
	{
		if (_bitset.eventually[i])
			eventualities.push_back(_subformulas[_lhs[i]]);
		else if (_bitset.until[i])
			eventualities.push_back(_subformulas[_rhs[i]]);
		else if (_bitset.not_until[i])
		{
			eventualities.push_back(_subformulas[_lhs[i]]);
			eventualities.push_back(_subformulas[_rhs[i]]);
		}
	}

	std::sort(eventualities.begin(), eventualities.end(), compareFunc);
	last = std::unique(eventualities.begin(), eventualities.end());
	eventualities.erase(last, eventualities.end());

	_bw_eventualities_lut = std::vector<FormulaID>(eventualities.size());
	for (uint64_t i = 0; i < eventualities.size(); ++i)
	{
		uint64_t position = static_cast<uint64_t>(std::lower_bound(_subformulas.begin(), _subformulas.end(), eventualities[i], compareFunc) - _subformulas.begin());
		_fw_eventualities_lut[position] = FormulaID(i);
		_bw_eventualities_lut[i] = FormulaID(position);
	}

	format::debug("Found {} eventualities", eventualities.size());

	if (_use_sat_solver)
	{
		format::debug("Generating clauses...");

		_clause_size = std::vector<uint64_t>(_number_of_formulas, 1);
		_clauses = std::vector<Clause>(_number_of_formulas);

		ClauseCounter counter;
		current_index = FormulaID(0);
		std::vector<Minisat::Lit> temp;
		std::function<void(const FormulaPtr&)> collect = [&](const FormulaPtr& f) // TODO: Fix the readability
		{
			assert(isa<Disjunction>(f));

			if (isa<Disjunction>(fast_cast<Disjunction>(f)->left()))
				collect(fast_cast<Disjunction>(f)->left());
			else
			{
				uint64_t index = static_cast<uint64_t>(std::lower_bound(_subformulas.begin(), _subformulas.end(), fast_cast<Disjunction>(f)->left(), compareFunc) - _subformulas.begin());
				if (isa<Negation>(fast_cast<Disjunction>(f)->left()) || (isa<Tomorrow>(fast_cast<Disjunction>(f)->left()) && (isa<Negation>(fast_cast<Tomorrow>(fast_cast<Disjunction>(f)->left())->formula()))))
					temp.push_back(Minisat::Lit(static_cast<int>(index - 1), true));
				else
					temp.push_back(Minisat::Lit(static_cast<int>(index)));
			}

			if (isa<Disjunction>(fast_cast<Disjunction>(f)->right()))
				collect(fast_cast<Disjunction>(f)->right());
			else
			{
				uint64_t index = static_cast<uint64_t>(std::lower_bound(_subformulas.begin(), _subformulas.end(), fast_cast<Disjunction>(f)->right(), compareFunc) - _subformulas.begin());
				if (isa<Negation>(fast_cast<Disjunction>(f)->right()) || (isa<Tomorrow>(fast_cast<Disjunction>(f)->right()) && (isa<Negation>(fast_cast<Tomorrow>(fast_cast<Disjunction>(f)->right())->formula()))))
					temp.push_back(Minisat::Lit(static_cast<int>(index - 1), true));
				else
					temp.push_back(Minisat::Lit(static_cast<int>(index)));
			}
		};

		for (const auto& f : _subformulas)
		{
			if (isa<Disjunction>(f))
				_clause_size[current_index] = counter.count(f);

			if (isa<Atom>(f) || isa<Always>(f) || isa<Eventually>(f) || isa<Until>(f))
			{
				Clause clause;
				clause.push(Minisat::Lit(static_cast<int>(current_index)));
				clause.moveTo(_clauses[current_index]);
			}
			else if (isa<Negation>(f)) // Note: Not until case implicitly handled here
			{
				Clause clause;
				clause.push(Minisat::Lit(static_cast<int>(current_index) - 1, true));
				clause.moveTo(_clauses[current_index]);
			}
			else if (isa<Tomorrow>(f))
			{
				Clause clause;
				if (isa<Negation>(fast_cast<Tomorrow>(f)->formula()))
					clause.push(Minisat::Lit(static_cast<int>(current_index) - 1, true));
				else
					clause.push(Minisat::Lit(static_cast<int>(current_index)));
				clause.moveTo(_clauses[current_index]);
			}
			else if (isa<Disjunction>(f))
			{
				temp.clear();
				collect(f);
				Clause clause;
				std::for_each(temp.begin(), temp.end(), [&](const Minisat::Lit& lit) { clause.push(lit); });
				clause.moveTo(_clauses[current_index]);
			}
			else if (isa<Then>(f))
				assert(false);
			else if (isa<Iff>(f))
				assert(false);

			// Note: We have nothing to do for conjunctions, because the formula is in CNF
			current_index++;
		}
	} // _use_sat_solver

	_has_eventually = _bitset.eventually.any();
	_has_until = _bitset.until.any();
	_has_not_until = _bitset.not_until.any();

	_stack.push(Frame(FrameID(0), _start_index, _number_of_formulas, _bw_eventualities_lut.size()));
	_state = State::INITIALIZED;

	format::debug("Solver initialized!");
}

void Solver::_add_formula_for_position(const FormulaPtr& formula, FormulaID position, FormulaID lhs, FormulaID rhs)
{
	switch (formula->type())
	{
		case Formula::Type::Atom:
			_bitset.atom[position] = true;
			_atom_set[position] = fast_cast<Atom>(formula)->name();
			break;

		case Formula::Type::Negation:
			if (isa<Until>(fast_cast<Negation>(formula)->formula()))
			{
				_bitset.not_until[position] = true;
				_lhs[position] = lhs;
				_rhs[position] = rhs;
				break;
			}
			_bitset.negation[position] = true;
			_lhs[position] = lhs;
			break;

		case Formula::Type::Tomorrow:
			_bitset.tomorrow[position] = true;
			_lhs[position] = lhs;
			break;

		case Formula::Type::Always:
			_bitset.always[position] = true;
			_lhs[position] = lhs;
			break;

		case Formula::Type::Eventually:
			_bitset.eventually[position] = true;
			_lhs[position] = lhs;
			break;

		case Formula::Type::Conjunction:
			_bitset.conjunction[position] = true;
			_lhs[position] = lhs;
			_rhs[position] = rhs;
			break;

		case Formula::Type::Disjunction:
			_bitset.disjunction[position] = true;
			_lhs[position] = lhs;
			_rhs[position] = rhs;
			break;

		case Formula::Type::Until:
			_bitset.until[position] = true;
			_lhs[position] = lhs;
			_rhs[position] = rhs;
			break;

		case Formula::Type::True:
		case Formula::Type::False:
		case Formula::Type::Iff:
		case Formula::Type::Then:
			assert(false);
			break;
	}
}

bool Solver::_check_contradiction_rule()
{
	const Frame& frame = _stack.top();

	_bitset.temporary = frame.formulas;
	_bitset.temporary &= _bitset.negation;
	_bitset.temporary >>= 1;
	_bitset.temporary &= frame.formulas;
	return _bitset.temporary.any();
}

bool Solver::_apply_conjunction_rule()
{
	Frame& frame = _stack.top();
	_bitset.temporary = frame.formulas;
	_bitset.temporary &= _bitset.conjunction;
	_bitset.temporary &= frame.to_process;

	if (!_bitset.temporary.any())
		return false;

	// TODO: find_first and find_next don't use __builtin_clz/__builtin_ctz, find if a custom implementation using them is faster
	size_t one = _bitset.temporary.find_first();
	while (one != Bitset::npos)
	{
		assert(_bitset.conjunction[one]);
		assert(frame.formulas[one]);
		assert(frame.to_process[one]);

		frame.formulas[_lhs[one]] = true;
		frame.formulas[_rhs[one]] = true;
		frame.to_process[one] = false;
		one = _bitset.temporary.find_next(one);
	}

	return true;
}

bool Solver::_apply_always_rule()
{
	Frame& frame = _stack.top();
	_bitset.temporary = frame.formulas;
	_bitset.temporary &= _bitset.always;
	_bitset.temporary &= frame.to_process;

	if (!_bitset.temporary.any())
		return false;

	size_t one = _bitset.temporary.find_first();
	while (one != Bitset::npos)
	{
		assert(_bitset.always[one]);
		assert(frame.formulas[one]);
		assert(frame.to_process[one]);

		frame.formulas[_lhs[one]] = true;
		assert(_bitset.tomorrow[one + 1] && _lhs[one + 1] == FormulaID(one));
		frame.formulas[one + 1] = true;
		frame.to_process[one] = false;
		one = _bitset.temporary.find_next(one);
	}

	return true;
}

#define APPLY_RULE(rule) \
bool Solver::_apply_##rule##_rule() \
{ \
        Frame& frame = _stack.top(); \
        _bitset.temporary = frame.formulas; \
        _bitset.temporary &= _bitset.rule; \
        _bitset.temporary &= frame.to_process; \
\
        size_t one = _bitset.temporary.find_first(); \
        if (one != Bitset::npos) \
        { \
                assert(_bitset.rule[one]); \
                assert(frame.formulas[one]); \
                assert(frame.to_process[one]); \
\
                frame.to_process[one] = false; \
                frame.choosenFormula = FormulaID(one); \
                frame.type = Frame::CHOICE; \
                return true; \
        } \
\
        return false; \
}

APPLY_RULE(disjunction)
APPLY_RULE(eventually)
APPLY_RULE(until)
APPLY_RULE(not_until)

#undef APPLY_RULE

namespace
{
volatile std::atomic_bool signal_status;
}

static void signal_handler(int /*signal*/)
{
	signal_status.store(true);
}

Solver::Result Solver::solution()
{
	if (_state == State::RUNNING || _state == State::DONE)
		return _result;

	if (_state == State::PAUSED)
		_rollback_to_latest_choice();

	_state = State::RUNNING;
	bool rules_applied;

	uint64_t maximum_steps = 1;
	uint64_t maximum_frames = 1;
	uint64_t total_frames = 1;
	uint64_t cross_by_contradiction = 0;
	uint64_t cross_by_prune = 0;

	std::signal(SIGINT, signal_handler);

loop:
	if (signal_status.load())
	{
		format::debug("/*-------------------------*/");
		format::debug("Total frames: {}", total_frames);
		format::debug("Maximum model size: {}", maximum_steps);
		format::debug("Maximum depth: {}", maximum_frames);
		format::debug("Cross by contradiction: {}", cross_by_contradiction);
		format::debug("Cross by prune: {}", cross_by_prune);
		format::debug("/*-------------------------*/");

		signal_status.store(false);
	}

	while (!_stack.empty())
	{
		Frame& frame = _stack.top();

		rules_applied = true;
		while (rules_applied)
		{
			rules_applied = false;

			if (__builtin_expect(frame.formulas.none(), 0))
			{
				_state = State::PAUSED;
				_result = Result::SATISFIABLE;
				_loop_state = frame.chain->id;

				format::debug("Total frames: {}", total_frames);
				format::debug("Maximum model size: {}", maximum_steps);
				format::debug("Maximum depth: {}", maximum_frames);
				format::debug("Cross by contradiction: {}", cross_by_contradiction);
				format::debug("Cross by prune: {}", cross_by_prune);

				return _result;
			}

			if (_check_contradiction_rule())
			{
				_rollback_to_latest_choice();
				++total_frames;
				++cross_by_contradiction;
				goto loop;
			}

			if (_apply_conjunction_rule())
				rules_applied = true;
			if (_apply_always_rule())
				rules_applied = true;

			if (!_should_use_sat_solver()) // TODO: Fix this condition
			{
				if (_apply_disjunction_rule())
				{
					Frame new_frame(frame);
					new_frame.formulas[_lhs[frame.choosenFormula]] = true;
					_stack.push(std::move(new_frame));

					++total_frames;
					if (_stack.size() > maximum_frames)
						maximum_frames = _stack.size();

					goto loop;
				}
			}

			if (_has_eventually && _apply_eventually_rule())
			{
				auto& ev = frame.eventualities[_fw_eventualities_lut[_lhs[frame.choosenFormula]]];
				if (__builtin_expect(ev.is_not_requested(), 0))
					ev.set_not_satisfied();

				Frame new_frame(frame);
				new_frame.formulas[_lhs[frame.choosenFormula]] = true;
				_stack.push(std::move(new_frame));

				++total_frames;
				if (_stack.size() > maximum_frames)
					maximum_frames = _stack.size();

				goto loop;
			}

			if (_has_until && _apply_until_rule())
			{
				auto& ev = frame.eventualities[_fw_eventualities_lut[_rhs[frame.choosenFormula]]];
				if (__builtin_expect(ev.is_not_requested(), 0))
					ev.set_not_satisfied();

				Frame new_frame(frame);
				new_frame.formulas[_rhs[frame.choosenFormula]] = true;
				_stack.push(std::move(new_frame));

				++total_frames;
				if (_stack.size() > maximum_frames)
					maximum_frames = _stack.size();

				goto loop;
			}

			if (_has_not_until && _apply_not_until_rule())
			{
				auto& ev = frame.eventualities[_fw_eventualities_lut[_lhs[frame.choosenFormula]]];
				if (__builtin_expect(ev.is_not_requested(), 0))
					ev.set_not_satisfied();
				ev = frame.eventualities[_fw_eventualities_lut[_rhs[frame.choosenFormula]]];
				if (__builtin_expect(ev.is_not_requested(), 0))
					ev.set_not_satisfied();

				Frame new_frame(frame);
				new_frame.formulas[_lhs[frame.choosenFormula]] = true;
				new_frame.formulas[_rhs[frame.choosenFormula]] = true;
				_stack.push(std::move(new_frame));

				++total_frames;
				if (_stack.size() > maximum_frames)
					maximum_frames = _stack.size();

				goto loop;
			}

			if (rules_applied)
				goto loop;

			if (_should_use_sat_solver())
			{
				/* https://github.com/niklasso/minisat-examples */
				frame.type = Frame::SAT;

				frame.solver = std14::make_unique<Minisat::Solver>();
				Minisat::Solver& solver = *frame.solver;
				std::for_each(_subformulas.begin(), _subformulas.end(), [&solver](const FormulaPtr& /*f*/) { solver.newVar(); });

				_bitset.temporary = _bitset.atom | _bitset.tomorrow | ((_bitset.atom << 1) & _bitset.negation) | _bitset.disjunction;
				//_bitset.temporary =  ~(_bitset.conjunction & _bitset.until & _bitset.not_until & _bitset.always & _bitset.eventually);
				//_bitset.temporary = ~_bitset.conjunction;
				_bitset.temporary &= frame.formulas;

				assert(frame.literals.empty());

				PrettyPrinter p;
				format::verbose(colors::Green, "Inserting formulas in the SAT solver: ");

				size_t one = _bitset.temporary.find_first();
				while (one != Bitset::npos)
				{
					solver.addClause(_clauses[one]);

					for (uint64_t i = 0; i < _clause_size[one]; ++i)
						frame.literals.push_back(Minisat::var(_clauses[one][static_cast<int>(i)]));

					if (_bitset.disjunction[one])
						frame.to_process[one] = false;

					format::verbose("{}", p.to_string(_subformulas[one]));

					one = _bitset.temporary.find_next(one);
				}

				std::sort(frame.literals.begin(), frame.literals.end());
				auto last = std::unique(frame.literals.begin(), frame.literals.end());
				frame.literals.erase(last, frame.literals.end());

				std::for_each(frame.literals.begin(), frame.literals.end(), [&](int /*lit*/) { solver.newVar(); });

				if (!solver.solve())
				{
					format::verbose("SAT says NO");
					frame.type = Frame::UNKNOWN; // This frame will be deallocated right now anyway
					_rollback_to_latest_choice();
					goto loop;
				}

				Frame new_frame(frame);
				Clause c;

				format::verbose(colors::Green, "Extracting formulas from the SAT solver: ");
				for (int l : frame.literals)
				{
					uint64_t id = static_cast<uint64_t>(l);

					if (solver.modelValue(l) == Minisat::l_True)
					{
						c.push(Minisat::Lit(l, true));
						new_frame.formulas[id] = true;

						format::verbose(colors::Green, "TRUE ");
						format::verbose("{}", p.to_string(_subformulas[id]));
					}
					else if (_bitset.negation[id + 1] || (isa<Tomorrow>(_subformulas[id + 1]) && isa<Negation>(fast_cast<Tomorrow>(_subformulas[id + 1])->formula()))) // Missing Gs and Fs
					{
						c.push(Minisat::Lit(l));
						new_frame.formulas[id + 1] = true;

						format::verbose(colors::Red, "FALSE ");
						format::verbose("{}", p.to_string(_subformulas[id + 1]));
					}
					else // TODO
					{
						//PrettyPrinter p;
						//p.print(_subformulas[id], true);
						//assert(false);
					}
				}

				solver.addClause(c);

				_stack.push(std::move(new_frame));
				goto loop;
			}
		}

		_update_eventualities_satisfaction();
		_update_history();

		bool loop_result = false;

		std::tie(loop_result, _loop_state) = _check_loop_rule();
		if (loop_result)
		{
			_result = Result::SATISFIABLE;
			_state = State::PAUSED;

			format::debug("Total frames: {}", total_frames);
			format::debug("Maximum model size: {}", maximum_steps);
			format::debug("Maximum depth: {}", maximum_frames);
			format::debug("Cross by contradiction: {}", cross_by_contradiction);
			format::debug("Cross by prune: {}", cross_by_prune);

			return _result;
		}

		if (_check_prune0_rule() || _check_prune_rule())
		{
			_rollback_to_latest_choice();
			++total_frames;
			++cross_by_prune;
			goto loop;
		}

		if (frame.id >= _maximum_depth)
		{
			_rollback_to_latest_choice();
			++total_frames;
			goto loop;
		}

		Frame new_frame(frame.id + 1, _number_of_formulas, frame.eventualities, &frame);
		_bitset.temporary = frame.formulas;
		_bitset.temporary &= _bitset.tomorrow;

		for (uint64_t i = 0; i < _number_of_formulas; ++i)
		{
			if (_bitset.temporary[i])
			{
				assert(frame.formulas[i]);
				assert(_bitset.tomorrow[i]);
				new_frame.formulas[_lhs[i]] = true;
			}
		}

		frame.type = Frame::STEP;
		_stack.push(std::move(new_frame));

		++total_frames;
		if (_stack.top().id > maximum_steps)
			maximum_steps = _stack.top().id;
	}

	_state = State::DONE;
	if (_result == Result::UNDEFINED)
		_result = Result::UNSATISFIABLE;

	format::debug("Total frames: {}", total_frames);
	format::debug("Maximum model size: {}", maximum_steps);
	format::debug("Maximum depth: {}", maximum_frames);
	format::debug("Cross by contradiction: {}", cross_by_contradiction);
	format::debug("Cross by prune: {}", cross_by_prune);

	return _result;
}

void Solver::_update_eventualities_satisfaction()
{
	Frame& frame = _stack.top();

	uint64_t i = 0;
	std::for_each(frame.eventualities.begin(), frame.eventualities.end(), [&](Eventuality& ev)
	{
		if (frame.formulas[_bw_eventualities_lut[i]])
			ev.set_satisfied(frame.id);
		++i;
	});
}

void Solver::_update_history()
{
	Frame* current_frame = _stack.top().chain;
	Frame& top_frame = _stack.top();

	while (current_frame)
	{
		if (current_frame->formulas == top_frame.formulas)
		{
			top_frame.prev = current_frame;
			top_frame.first = current_frame->first;
			return;
		}

		current_frame = current_frame->chain;
	}

	top_frame.prev = &top_frame;
	top_frame.first = &top_frame;
}

std::tuple<bool, FrameID> Solver::_check_loop_rule() const
{
	const Frame& top_frame = _stack.top();
	const FrameID first_frame_id = top_frame.first->id;

	if (top_frame.first == &top_frame)
		return std::tuple<bool, FrameID>{false, FrameID(0)};

	bool ret = std::all_of(top_frame.eventualities.begin(), top_frame.eventualities.end(), [first_frame_id](const Eventuality& ev)
	{
		return ev.is_not_requested() || (ev.is_satisfied() && ev.id() > first_frame_id);
	});

	return std::tuple<bool, FrameID>{ ret, first_frame_id };
}

bool Solver::_check_prune0_rule() const
{
	const Frame& top_frame = _stack.top();
	const FrameID prev_frame_id = top_frame.prev->id;

	if (top_frame.prev == &top_frame)
		return false;

	return !top_frame.eventualities.empty() &&
		std::none_of(top_frame.eventualities.begin(), top_frame.eventualities.end(), [&, prev_frame_id](const Eventuality& ev)
	{
		if (ev.is_not_requested() || ev.is_not_satisfied())
			return false;

		return ev.id() > prev_frame_id;
	});
}

bool Solver::_check_prune_rule() const
{
	const Frame& top_frame = _stack.top();
	//const FrameID prev_frame_id = top_frame.prev->id;
	//const FrameID first_frame_id = top_frame.first->id;

	if (top_frame.prev == top_frame.first)
		return false;

	uint64_t i = 0;
	return std::all_of(top_frame.eventualities.begin(), top_frame.eventualities.end(), [&top_frame, &i](const Eventuality& ev)
	{
		if (ev.is_not_requested() || ev.is_not_satisfied() || ev.id() <= top_frame.prev->id)
		{
			++i;
			return true;
		}

		bool ret = top_frame.prev->eventualities[i].is_satisfied() && top_frame.prev->eventualities[i].id() > top_frame.first->id;
		++i;
		return ret;
	});
}

bool Solver::_check_my_prune() const
{
	const Frame& top_frame = _stack.top();

	if (top_frame.prev == &top_frame)
		return false;

	return std::any_of(top_frame.eventualities.begin(), top_frame.eventualities.end(), [](const Eventuality& ev)
	{
		return ev.is_not_satisfied();
	});
}

void Solver::_rollback_to_latest_choice()
{
	while (!_stack.empty())
	{
		if (_stack.top().type == Frame::CHOICE && _stack.top().choosenFormula != FormulaID::max())
		{
			Frame& top = _stack.top();;
			Frame new_frame(top);

			if (_bitset.disjunction[top.choosenFormula])
				new_frame.formulas[_rhs[top.choosenFormula]] = true;
			else if (_bitset.eventually[top.choosenFormula])
			{
				new_frame.formulas[top.choosenFormula + 1] = true;
				assert(_bitset.tomorrow[top.choosenFormula + 1] && _lhs[top.choosenFormula + 1] == top.choosenFormula);
			}
			else if (_bitset.until[top.choosenFormula])
			{
				new_frame.formulas[_lhs[top.choosenFormula]] = true;
				if (_bitset.tomorrow[top.choosenFormula + 1])
				{
					new_frame.formulas[top.choosenFormula + 1] = true;
					assert(_lhs[top.choosenFormula + 1] == top.choosenFormula);
				}
				else
				{
					new_frame.formulas[top.choosenFormula + 2] = true;
					assert(_lhs[top.choosenFormula + 2] == top.choosenFormula);
				}
			}
			else if (_bitset.not_until[top.choosenFormula]) // TODO: Negation are not always in the + 1 position
			{
				new_frame.formulas[_rhs[top.choosenFormula]] = true;
				if (_bitset.tomorrow[top.choosenFormula + 1])
				{
					new_frame.formulas[top.choosenFormula + 1] = true;
					assert(_lhs[top.choosenFormula + 1] == top.choosenFormula);
				}
				else
				{
					new_frame.formulas[top.choosenFormula + 2] = true;
					assert(_lhs[top.choosenFormula + 2] == top.choosenFormula);
				}
			}
			else
				assert(false);

			top.choosenFormula = FormulaID::max();
			_stack.push(std::move(new_frame));

			return;
		}
		else if (_stack.top().type == Frame::SAT)
		{
			//std::cout << "\033[0;31m" << "ROLLBACK" << "\033[0m" << std::endl;
			Minisat::Solver& solver = *_stack.top().solver;

			bool satisfiable = solver.solve();
			if (satisfiable)
			{
				PrettyPrinter p;

				Frame new_frame(_stack.top());
				Clause c;

				//std::cout << "\033[0;32m" << "Extracting formulas from the SAT solver: " << "\033[0m" << std::endl;
				for (int l : _stack.top().literals)
				{
					uint64_t id = static_cast<uint64_t>(l);

					if (solver.modelValue(l) == Minisat::l_True)
					{
						c.push(Minisat::Lit(l, true));
						new_frame.formulas[id] = true;

						//std::cout << "\033[0;32m" << "TRUE " << "\33[0m";
						//p.print(_subformulas[id], true);
					}
					else if (_bitset.negation[id + 1] || (isa<Tomorrow>(_subformulas[id + 1]) && isa<Negation>(fast_cast<Tomorrow>(_subformulas[id + 1])->formula()))) // Missing Gs and Fs
					{
						c.push(Minisat::Lit(l));
						new_frame.formulas[id + 1] = true;

						//std::cout << "\033[0;31m" << "FALSE " << "\33[0m";
						//p.print(_subformulas[id + 1], true);
					}
					else // TODO
					{
						//PrettyPrinter p;
						//p.print(_subformulas[id + 1], true);
						//assert(false);
					}
				}

				solver.addClause(c);

				_stack.push(std::move(new_frame));
				return;
			}
		}

		_stack.pop();
	}
}

// TODO: This crash when the formula simplifies to True
ModelPtr Solver::model()
{
	if (_state != State::PAUSED)
		return nullptr;

	if (_result == Result::UNSATISFIABLE || _result == Result::UNDEFINED)
		return nullptr;

	ModelPtr model = std::make_shared<Model>();

	if (_subformulas.size() == 1 && isa<True>(_subformulas[0]))
	{
		model->loop_state = 0;
		model->states.push_back({ Literal(u8"\u22a4") });
		return model;
	}

	uint64_t i = 0;
	for (const auto& frame : Container(_stack))
	{
		if (frame.type == Frame::CHOICE || frame.type == Frame::SAT)
			continue;

		LTL::detail::State state;
		for (uint64_t j = 0; j < _number_of_formulas; ++j)
		{
			if (frame.formulas[j])
			{
				if (_atom_set.find(FormulaID(j)) != _atom_set.end())
					state.insert(Literal(_atom_set.find(FormulaID(j))->second));
				else if (_bitset.negation[j] && _atom_set.find(_lhs[j]) != _atom_set.end())
					state.insert(Literal(_atom_set.find(FormulaID(_lhs[j]))->second, false));
			}
		}

		std::cout << std::endl;

		model->states.push_back(state);
		++i;
	}

	if (_stack.top().id != 0)
		model->states.pop_back();
	model->loop_state = _loop_state;

	return model;
}

bool Solver::_should_use_sat_solver()
{
	assert(_stack.top().type == Frame::SAT || _stack.top().type == Frame::UNKNOWN);

	if (!_use_sat_solver)
		return false;

	_bitset.temporary = _stack.top().formulas;
	_bitset.temporary &= _stack.top().to_process;
	_bitset.temporary &= _bitset.disjunction;
	return _bitset.temporary.any();
}

FormulaPtr inline Solver::Formula() const
{
	return _formula;
}

void Solver::__dump_current_formulas() const
{
	PrettyPrinter p;
	for (uint64_t i = 0; i < _subformulas.size(); ++i)
		if (_stack.top().formulas[i])
			format::verbose("{}", p.to_string(_subformulas[i]));
	std::cout << std::endl;
}

void Solver::__dump_current_eventualities() const
{
	PrettyPrinter p;
	for (uint64_t i = 0; i < _bw_eventualities_lut.size(); ++i)
		format::verbose("{} : {}", _subformulas[_bw_eventualities_lut[i]], static_cast<uint64_t>(_stack.top().eventualities[i].id()));
}

void Solver::__dump_eventualities(FrameID id) const
{
	PrettyPrinter p;
	Frame* current_frame = _stack.top().chain;
	while (current_frame)
	{
		if (current_frame->id == id)
			break;

		current_frame = current_frame->chain;
	}

	for (uint64_t i = 0; i < _bw_eventualities_lut.size(); ++i)
		format::verbose("{} : {}", _subformulas[_bw_eventualities_lut[i]], static_cast<uint64_t>(Container(_stack)[id].eventualities[i].id()));
}

}
}
