cmake_minimum_required (VERSION 2.8.11)
project (lib CXX)

# Boost library
add_definitions ("-DBOOST_ALL_NO_LIB")

if(STATIC_LIBS MATCHES "y")
  set (Boost_USE_STATIC_LIBS ON)
  set (Boost_USE_STATIC_RUNTIME ON)
else()
  set (Boost_USE_STATIC_LIBS OFF)
  set (Boost_USE_STATIC_RUNTIME OFF)
endif()

set (Boost_USE_MULTITHREADED ON)
set (Boost_USE_DEBUG_RUNTIME OFF)
find_package (Boost 1.36.0 REQUIRED COMPONENTS system thread)

#
# Leviathan library
#
set (
  SRC 
  src/ast/formula.cpp
  src/ast/generator.cpp
  src/ast/simplifier.cpp
  src/ast/translator.cpp
  src/ast/pretty_printer.cpp
  src/solver.cpp
  src/parser/lex.cpp
  src/parser/parser.cpp
  src/format.cpp
)

set (
  HEADERS
  include/formula.hpp
  include/frame.hpp
  include/identifiable.hpp
  include/leviathan.hpp
  include/model.hpp
  include/parser.hpp
  include/pretty_printer.hpp
  include/simplifier.hpp
  src/ast/generator.hpp
  src/ast/translator.hpp
  include/solver.hpp
  include/visitor.hpp
  include/format.hpp
)

# Here adding the headers is redundant, but makes them appear in 
# project files made by IDE generators (Xcode, VS, ecc...)
add_library (leviathan STATIC ${SRC} ${HEADERS})

target_include_directories(
  leviathan
  PRIVATE src/
  PUBLIC
    include/
    src/new_parser/
    "${CPPUTILS_HEADERS}"
    "${TCLAP_HEADERS}"
    "${STDOPTIONAL_HEADERS}"
    "${Boost_INCLUDE_DIRS}"
)

# Dependencies
target_link_libraries (leviathan cppformat ${Boost_LIBRARIES})

# General options
set(MSVC_OPTIONS /MP)

set(GNU_OPTIONS -std=c++11)

set(CLANG_OPTIONS -std=c++11)

target_compile_options(
  leviathan PUBLIC
  "$<$<CXX_COMPILER_ID:MSVC>:${MSVC_OPTIONS}>"
  "$<$<CXX_COMPILER_ID:GNU>:${GNU_OPTIONS}>"
  "$<$<CXX_COMPILER_ID:Clang>:${CLANG_OPTIONS}>"
)

# Warnings
set(
  CLANG_WARNINGS -Weverything -pedantic -Werror
  -Wno-c++98-compat -Wno-c++98-compat-pedantic
  -Wno-padded -Wno-weak-vtables -Wno-unknown-pragmas
  -Wno-exit-time-destructors -Wno-switch-enum
  -Wno-undefined-var-template
  -Wno-undefined-func-template
  -Wno-reserved-id-macro -Wno-deprecated
  -Wno-old-style-cast -Wno-documentation
  -Wno-documentation-unknown-command
  -Wno-sign-conversion
  -Wno-global-constructors -Wno-extra-semi
  -Wno-unknown-warning-option
)

set(
  GNU_WARNINGS -Wall -Wextra -pedantic -Werror 
  -Wno-pragmas -Wno-unknown-pragmas
)

set(MSVC_WARNINGS /wd4068 /wd4702 /W4 /WX)

target_compile_options(
  leviathan PUBLIC 
  "$<$<CXX_COMPILER_ID:MSVC>:${MSVC_WARNINGS}>"
  "$<$<CXX_COMPILER_ID:GNU>:${GNU_WARNINGS}>"
  "$<$<CXX_COMPILER_ID:Clang>:${CLANG_WARNINGS}>"
)


