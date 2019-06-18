# Leviathan

[![Build Status](https://travis-ci.org/Corralx/leviathan.svg?branch=master)](https://travis-ci.org/Corralx/leviathan)

## About

Leviathan is an implementation of a tableau method for [LTL](https://en.wikipedia.org/wiki/Linear_temporal_logic) satisfiability checking based on the paper "A new rule for a traditional tree-style LTL tableau" by [Mark Reynolds](http://www.csse.uwa.edu.au/~mark/research/Online/ltlsattab.html).

This work has been accepted for publication at [IJCAI-16](http://ijcai-16.org/index.php/welcome/view/home) and a preprint can be found [here](http://corralx.me/public/leviathan_preprint.pdf).

### WARNING

If you used or are using the tool and checked it out before June 6, 2019, please update to the latest
revision. A nasty bug was fixed that could cause some satisfiable formulas to be
reported as unsatisfiable.

Thanks to Nikhil Babu from the ANU for reporting it.

### Installation

Example installation instructions for Ubuntu 18.04 are provided below. Adapt
them to your distribution.

Clone the repository:

```
$ git clone --recursive git@github.com:Corralx/leviathan.git
$ cd leviathan
```

Install dependencies:
```
$ sudo apt-get install build-essential cmake libboost-thread-dev
```

Configure the compilation (a separate build directory is recommended):
```
$ mkdir build && cd build
$ ../configure
```

Build, test, and install:
```
$ make
$ make test
$ sudo make install
```

### Usage

Just call the tool passing the path to a file which contains the formulas to check. The parser is very flexible and supports every common LTL syntax used in other tools. Every line of the file is treated as an independent formula, so more than one formula at a time can be checked.

Moreover several command line options are present:

* **-l** or **--ltl** let the user specify the formula directly on the command line
* **-m** or **--model** generates and prints a model of the formula, if any
* **-p** or **--parsable** generates machine-parsable output
* **--maximum-depth** specifies the maximum depth of the tableau (and therefore the maximum size of the model)
* **-v \<0-5>** or **--verbosity \<0-5>** specifies the verbosity of the output
* **--version** prints the current version of the tool
* **-h** or **--help** displays the usage message

### Usage example

The following sample:
> ./checker.exe --ltl "G (req => X grant) & req" --model

Outputs the following answer:
> The formula is satisfiable!<br>
> The following model was found:<br>
> State 0:<br>
> req<br>
> State 1:<br>
> grant, !req<br>
> State 2:<br>
> !req<br>
> Loops to state 2

While with the addition of the `--parsable` flag the output is the following:
> SAT;{req} -> {grant,!req} -> {!req} -> #2

### Compatibility

Leviathan has been written since the beginning with the portability in mind. The only prerequisite is a fairly recent C++11 compiler. It is known to work on Windows, Mac OS X and Linux and compiles correctly under MSVC 2015, GCC 4.8+ and Clang 3.5+.

## Future Work

* Refactor the code to enhance redability and simplify new features addition
* Remove the dependency on Boost by reimplementing a custom stack and allocator
* Let the user choose the order of application of the tableau rules
* Investigate the use of caching to quickly prune identical subtrees

See the [TODO](https://github.com/Corralx/leviathan/blob/master/TODO.md) file for a more complete recap of the work in progress!

## License

It is licensed under the very permissive [MIT License](https://opensource.org/licenses/MIT).
For the complete license see the provided [LICENSE](https://github.com/Corralx/leviathan/blob/master/LICENSE.md) file in the root directory.

## Thanks

Several open-source third-party libraries are currently used in this project:
* [Boost](http://www.boost.org/) for the dynamic bitset and pool allocator, and
  for the `boost::optional` type
* [fmtlib](https://github.com/fmtlib/fmt) to format the output
* [tclap](http://tclap.sourceforge.net/) to parse the command line arguments
