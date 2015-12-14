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

#include "leviathan.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#define strerror_safe(error_msg, error_length, errno) \
  (strerror_s(error_msg, error_length, errno))
#else
#define strerror_safe(error_msg, error_length, errno) \
  (strerror_r(errno, error_msg, error_length))
#endif

#include "optional.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wweak-vtables"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wdocumentation-unknown-command"
#pragma GCC diagnostic ignored "-Wmissing-noreturn"

#include "tclap/CmdLine.h"

#pragma GCC diagnostic pop

/*
 Rework the driver program.
 - Command-line interface and console issues:
 * Colored output (factor out into the format namespace)
 * Choose the level of verbosity on the command line
 * --timings option to enable time measurements output
 - Actions:
 * Parse the formula
 * Parse the test data associated to the formula, if the --test option
   was given
 * Initialize the solver and solve
 * Give the output (parsable output should have same format as test files)
 - Time measurement:
 * On parsing
 * On solving
 * Parsable output
*/

namespace format = LTL::format;
using namespace format::colors;
using std::experimental::optional;
using std::experimental::nullopt;

static constexpr auto leviathan_version = "0.3.0";

/*
 * Command line arguments. It is not so bad to make them global objects
 * since no one will ever write to them excepting the CmdLine::parse()
 * function.
 * Being able to access to command line arguments from everywhere aids
 * modularity in the rest of the file.
 *
 * If you add a new parameter, remember to register it in main()
 */

/*
 * To register the type for verbosity arguments
 */
namespace TCLAP {
template <>
struct ArgTraits<format::LogLevel> : ValueLikeTrait {
};
}

namespace Args {

static TCLAP::UnlabeledValueArg<std::string> filename(
  "filename", "The name of the file to load the formulas from", false, "-",
  "path");

static TCLAP::ValueArg<std::string> ltl(
  "l", "ltl",
  "The LTL formula to solve, provided directly on the command line", false, "",
  "LTL formula");

static TCLAP::SwitchArg model(
  "m", "model",
  "Generates and prints a model of the formula, when satisfiable", false);

static TCLAP::SwitchArg parsable("p", "parsable",
                                 "Generates machine-parsable output", false);

static TCLAP::SwitchArg test(
  "t", "test",
  "Test the checker answer. In this mode, the '-f' flag is mandatory. The "
  "given filename will be read for the formula, together with a .answer "
  "file "
  "named after it, containing the correct answer. The .answer file must "
  "contain the syntactic representation of the correct model of the given "
  "formula, if the formula is satisfiable, or be empty otherwise.",
  false);

static TCLAP::ValueArg<format::LogLevel> verbosity(
  "v", "verbosity",
  "The level of verbosity of solver's output."
  "The higher the value, the more verbose the output will be. A verbosity of "
  "zero means total silence, even for error messages. Five or higher means "
  "total annoyance.",
  false, format::Message, "number between 0 and 5");

static TCLAP::SwitchArg sat(
  "s", "sat",
  "Uses sat solver to speed up propositional subformulas processing", false);

static TCLAP::ValueArg<uint64_t> depth(
  "", "maximum-depth",
  "The maximum depth to descend into the tableaux (aka the maximum size of "
  "the model)",
  false, std::numeric_limits<uint64_t>::max(), "number");
}

void solve(std::string const &, optional<size_t> current = nullopt);
void print_progress_status(std::string, size_t);
void batch(std::string const &);

// We suppose 80 columns is a good width
void print_progress_status(std::string formula, size_t current)
{
  if (Args::parsable.isSet())
    return;

  std::string msg = format::format("Solving formula n° {}: ", current);

  /*
   * Formatting the formula on one line and printing up to 80 columns
   */
  formula.erase(std::remove(begin(formula), end(formula), '\n'), end(formula));

  std::string ellipses;
  if (formula.length() + msg.length() > 80) {
    formula = formula.substr(0, 80 - msg.length() - 3);
    ellipses = "...";
  }

  format::message("{}{}{}", msg, formula, ellipses);
}

void solve(const std::string &input, optional<size_t> current)
{
  if (current)
    print_progress_status(input, *current);

  optional<LTL::FormulaPtr> parsed = LTL::parse(input);

  if (!parsed) {
    format::error("Syntax error in formula{}. Skipping...",
                  current ? format::format(" n° {}", *current) : "");
    return;
  }

  LTL::FormulaPtr formula = *parsed;

  LTL::Solver solver(formula, LTL::FrameID(Args::depth.getValue()),
                     Args::sat.getValue());

  solver.solution();

  bool sat = solver.satisfiability() == LTL::Solver::Result::SATISFIABLE;

  if (Args::parsable.isSet())
    format::message("{}", sat ? colored(Green, "SAT") : colored(Red, "UNSAT"));
  else
    format::message("The formula is {}!", sat ? colored(Green, "satisfiable")
                                              : colored(Red, "unsatisfiable"));

  if (sat && Args::model.isSet()) {
    LTL::ModelPtr model = solver.model();

    if (!Args::parsable.isSet())
      format::message("The following model was found:");

    format::message("{}", model_format(model, Args::parsable.isSet()));
  }
}

void batch(std::string const &filename)
{
  std::ifstream file(filename, std::ios::in);

  if (!file) {
    static constexpr size_t error_length = 128;
    char error_msg[error_length];
    strerror_safe(error_msg, error_length, errno);
    format::fatal("Unable to open the file \"{}\": {}", filename, error_msg);
    return;
  }

  std::string line;
  size_t line_number = 1;
  while (std::getline(file, line)) {
    solve(line, line_number);
    ++line_number;
  }
}

int main(int argc, char *argv[])
{
  TCLAP::CmdLine cmd("A simple LTL satisfiability checker", ' ',
                     leviathan_version);

  using namespace Args;

  cmd.add(depth);
  cmd.add(sat);
  cmd.add(verbosity);
  cmd.add(parsable);
  cmd.add(model);
  cmd.add(ltl);
  cmd.add(filename);

  cmd.parse(argc, argv);

  // Setup the verbosity first of all
  format::set_verbosity_level(verbosity.getValue());

  format::verbose("Verbose message. I told you this would be very verbose.");

  // Begin to process inputs
  if (ltl.isSet())
    solve(ltl.getValue(), 1);
  else
    batch(filename.getValue());

  return 0;
}
