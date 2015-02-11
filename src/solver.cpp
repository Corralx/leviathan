#include "solver.hpp"

#include "generator.hpp"
#include "utility.hpp"
#include "pretty_printer.hpp"
#include "clause_counter.hpp"
#include <stack>
#include <iostream>
#include <cassert>
#include <csignal>
#include <atomic>

namespace LTL
{

namespace detail
{

Solver::Solver(FormulaPtr formula, FrameID maximum_depth, uint32_t backtrack_probability, uint32_t minimum_backtrack, uint32_t maximum_backtrack)
        : _formula(formula), _maximum_depth(maximum_depth), _backtrack_probability(backtrack_probability), _minimum_backtrack(minimum_backtrack),
          _maximum_backtrack(maximum_backtrack), _state(State::UNINITIALIZED), _result(Result::UNDEFINED), _start_index(0), _loop_state(0)
{
        if (_backtrack_probability > 100)
                _backtrack_probability = 100;

        if (_maximum_backtrack > 100)
                _maximum_backtrack = 100;
        
        if (_minimum_backtrack > _maximum_backtrack)
                _minimum_backtrack = _maximum_backtrack;

        _mt = std::mt19937((std::random_device())());
        _backtrack_probability_rand = std::uniform_int_distribution<uint32_t>(0, 100);
        _backtrack_percentage_rand = std::uniform_int_distribution<uint32_t>(_minimum_backtrack, _maximum_backtrack);

        _initialize();
}

void Solver::_initialize()
{
        std::cout << (sizeof(Frame)) << std::endl;
        std::cout << (sizeof(Bitset) * 2 + sizeof(std::vector<Eventuality>) + sizeof(FrameID) + sizeof(FormulaID) + sizeof(bool) + sizeof(Frame*)) << std::endl;
        std::cout << "Initializing solver..." << std::endl;

        _atom_set.clear();

        std::cout << "Simplifing formula..." << std::endl;

        Simplifier simplifier;
        _formula = simplifier.simplify(_formula);

        std::cout << "Generating subformulas..." << std::endl;

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

        std::function<bool(FormulaPtr, FormulaPtr)> compareFunc = [&compareFunc](const FormulaPtr a, const FormulaPtr b)
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
        
        return a->type() < b->type();
        };

        std::sort(_subformulas.begin(), _subformulas.end(), compareFunc);

        auto last = std::unique(_subformulas.begin(), _subformulas.end());
        _subformulas.erase(last, _subformulas.end());

        std::cout << "Found " << _subformulas.size() << " subformulas" << std::endl;
        std::cout << "Building data structure..." << std::endl;

        FormulaID current_index(0);
    
        _number_of_formulas = _subformulas.size();
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

        for (auto f : _subformulas)
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

                if (left)
                        lhs = FormulaID(static_cast<uint64_t>(std::lower_bound(_subformulas.begin(), _subformulas.end(), left, compareFunc) - _subformulas.begin()));
                if (right)
                        rhs = FormulaID(static_cast<uint64_t>(std::lower_bound(_subformulas.begin(), _subformulas.end(), right, compareFunc) - _subformulas.begin()));

                _add_formula_for_position(f, current_index++, lhs, rhs);
        }

        std::cout << "Generating eventualities..." << std::endl;

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
                uint64_t position = static_cast<uint64_t>(std::lower_bound(eventualities.begin(), eventualities.end(), eventualities[i], compareFunc) - eventualities.begin());
                _fw_eventualities_lut[position] = FormulaID(i);
                _bw_eventualities_lut[i] = FormulaID(position);
        }

        std::cout << "Found " << eventualities.size() << " eventualities" << std::endl;
        std::cout << "Generating clauses..." << std::endl;

        _clause_size = std::vector<uint64_t>(_number_of_formulas, 1);
        ClauseCounter counter;
        current_index = FormulaID(0);
        for (auto f : _subformulas)
        {
                if (isa<Disjunction>(f))
                        _clause_size[current_index] = counter.count(f);
                
                current_index++;
        }

        _stack.push(Frame(FrameID(0), _start_index, _number_of_formulas, _bw_eventualities_lut.size()));
        _state = State::INITIALIZED;

        std::cout << "Solver initialized!" << std::endl;
}

void Solver::_add_formula_for_position(const FormulaPtr formula, FormulaID position, FormulaID lhs, FormulaID rhs)
{
         switch (formula->type())
        {
                case Formula::Type::Atom:
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
                one = _bitset.temporary.find_next(one + 1);
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
                one = _bitset.temporary.find_next(one + 1);
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
                frame.choice = true; \
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

Solver::Result Solver::solution()
{
        if (_state == State::RUNNING || _state == State::DONE)
                return _result;

        _state = State::RUNNING;
        bool rules_applied;

        if (_state == State::PAUSED)
                _rollback_to_latest_choice();

loop:
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
                                return _result;
                        }

                        if (_check_contradiction_rule())
                        {
                                _rollback_to_latest_choice();
                                goto loop;
                        }

                        if (_apply_conjunction_rule())
                                rules_applied = true;
                        if (_apply_always_rule())
                                rules_applied = true;

                        if (_apply_disjunction_rule())
                        {
                                Frame new_frame(frame.id, frame);
                                new_frame.formulas[_lhs[frame.choosenFormula]] = true;
                                _stack.push(std::move(new_frame));

                                goto loop;
                        }

                        if (_apply_eventually_rule())
                        {
                                auto& ev = frame.eventualities[_fw_eventualities_lut[_lhs[frame.choosenFormula]]];
                                if (__builtin_expect(ev.is_not_requested(), 0))
                                        ev.set_not_satisfied();

                                Frame new_frame(frame.id, frame);
                                new_frame.formulas[_lhs[frame.choosenFormula]] = true;
                                _stack.push(std::move(new_frame));

                                goto loop;
                        }

                        if (_apply_until_rule())
                        {
                                auto& ev = frame.eventualities[_fw_eventualities_lut[_rhs[frame.choosenFormula]]]; 
                                if (__builtin_expect(ev.is_not_requested(), 0))
                                        ev.set_not_satisfied();

                                Frame new_frame(frame.id, frame);
                                new_frame.formulas[_rhs[frame.choosenFormula]] = true;
                                _stack.push(std::move(new_frame));

                                goto loop;
                        }

                        if (_apply_not_until_rule())
                        {
                                auto& ev = frame.eventualities[_fw_eventualities_lut[_lhs[frame.choosenFormula]]]; 
                                if (__builtin_expect(ev.is_not_requested(), 0))
                                        ev.set_not_satisfied();
                                ev = frame.eventualities[_fw_eventualities_lut[_rhs[frame.choosenFormula]]]; 
                                if (__builtin_expect(ev.is_not_requested(), 0))
                                        ev.set_not_satisfied();

                                Frame new_frame(frame.id, frame);
                                new_frame.formulas[_lhs[frame.choosenFormula]] = true;
                                new_frame.formulas[_rhs[frame.choosenFormula]] = true;
                                _stack.push(std::move(new_frame));

                                goto loop;
                        }
                }

                _update_eventualities_satisfaction();

                // LOOP rule
                const Frame* repFrame1 = nullptr, *repFrame2 = nullptr;
                const Frame* currFrame = frame.chain;

                //FrameID min_frame;

                // Heuristics: OCCASIONAL LOOKBACK
                if (_backtrack_probability_rand(_mt) > _backtrack_probability)
                        goto step_rule;
                
                // Heuristics: PARTIAL LOOKBACK
                //min_frame = FrameID(static_cast<uint64_t>(static_cast<float>(_backtrack_percentage_rand(_mt)) / 100.f * static_cast<uint64_t>(currFrame->id)));

                while (currFrame)
                {
                        // Heuristics: PARTIAL LOOKBACK
                        //if (currFrame->id < min_frame)
                                //break;

                        if (frame.formulas.is_subset_of(currFrame->formulas))
                        {
                                // TODO: Can this be done in a cheaper way? Probably yes
                                // if (eventualities_satisfied(frame, currFrame))
                                bool all_satisfied = true;
                                for (uint64_t i = 0; i < _bw_eventualities_lut.size(); ++i)
                                {
                                        const Eventuality& ev = frame.eventualities[i];
                                        if (!(ev.is_satisfied() && ev.id() >= currFrame->id))
                                        {
                                                all_satisfied = false;
                                                break;
                                        }
                                }

                                if (__builtin_expect(all_satisfied, 0))
                                {
                                        _result = Result::SATISFIABLE;
                                        _state = State::PAUSED;
                                        _loop_state = currFrame->id;
                                        return _result;
                                }

                                //if (!frame.formulas.is_proper_subset_of(currFrame->formulas)) // Alternative: seems to be completely the same in terms of performance
                                if (frame.formulas == currFrame->formulas) // STEP rule check
                                {
                                        if (!repFrame1)
                                                repFrame1 = currFrame;
                                        else if (!repFrame2)
                                                repFrame2 = currFrame;
                                }
                        }
                        currFrame = currFrame->chain;
                }

                // REP rule application
                if (repFrame1 && repFrame2)
                {
                        _rollback_to_latest_choice();
                        goto loop;
                }

// Heuristics: OCCASIONAL LOOKBACK
step_rule:

                if (frame.id >= _maximum_depth)
                {
                        _rollback_to_latest_choice();
                        goto loop;
                }

                // STEP rule
                Frame new_frame(frame.id + 1, _number_of_formulas, frame.eventualities, &frame);
                _bitset.temporary = frame.formulas;
                _bitset.temporary &= _bitset.tomorrow;

                /* TODO: This doesn't work for w/e reason. Investigate
                size_t p = _bitset.temporary.find_first();
                while (p != Bitset::npos)
                {
                        assert(_bitset.tomorrow[p]);
                        assert(frame.formulas[p]);
                
                        new_frame.formulas[_lhs[p]] = true;
                        p = _bitset.temporary.find_next(p + 1);
                }
                */
                
                for (uint64_t i = 0; i < _number_of_formulas; ++i)
                {
                        if (_bitset.temporary[i])
                        {
                                assert(frame.formulas[i]);
                                assert(_bitset.tomorrow[i]);
                                new_frame.formulas[_lhs[i]] = true;
                        }
                }

                _stack.push(std::move(new_frame));
        }

        _state = State::DONE;
        if (_result == Result::UNDEFINED)
                _result = Result::UNSATISFIABLE;

        return _result;
}

 void Solver::_update_eventualities_satisfaction()
 {
        Frame& frame = _stack.top();

        std::for_each(frame.eventualities.begin(), frame.eventualities.end(), [&, i = 0] (Eventuality& ev) mutable
        {
                if (frame.formulas[i])
                        ev.set_satisfied(frame.id);
                ++i;
        });
 }

void Solver::_rollback_to_latest_choice()
{
        while (!_stack.empty())
        {
                if (_stack.top().choice && _stack.top().choosenFormula != FormulaID::max())
                {
                        Frame& top = _stack.top();;
                        Frame new_frame(top.id, top);

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
                        else if (_bitset.not_until[top.choosenFormula])
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
                        {
                                assert(false);
                        }

                        top.choosenFormula = FormulaID::max();
                        _stack.push(std::move(new_frame));

                        return;
                }

                _stack.pop();
        }
}

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
                model->states.push_back({ Literal("\u22a4") });
                return model;
        }

        uint64_t i = 0;
        for (const auto& frame : Container(_stack))
        {
                if (frame.choice)
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
                
                model->states.push_back(state);
                ++i;
        }
        
        model->states.pop_back();
        model->loop_state = _loop_state;

        // TODO: Optimize model to compensate heuristics. How to do it conservatively?
        /* Heuristics: OPTIMIZE MODEL
        std::vector<int64_t> toRemove;
        for (int64_t i = loopTo; i >= 0; --i) 
        {
                    if (std::all_of(model[loopTo].begin(), model[loopTo].end(), [&] (LTL::FormulaPtr f)
                    {
                            if (model[i].find(f) != model[i].end())
                                    return true;
                            return false;
                    }))
                    toRemove.push_back(i);
        }

        for (auto i : toRemove)
                model.erase(model.begin() + i);
        */

        return model;
}

}
}
