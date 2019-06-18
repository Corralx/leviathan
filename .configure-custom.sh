#!/usr/bin/env bash

#
# This file is sourced by the configure script,
# it's useless to call it directly
#

#
# Additional configure options
#
DISABLE_VARS="tests|no|ENABLE_TESTS"

WITH_VARS="static-libs|yes|STATIC_LIBS"

#
# Chose compiler from Travis
#
if [ "x$TRAVIS_CLANG_VERSION" != "x" ]; then

  if [ "$CC" = "clang" ]; then
    CC=clang-$TRAVIS_CLANG_VERSION
  fi

  if [ "$CXX" = "clang" ]; then
    CXX=clang++-$TRAVIS_CLANG_VERSION
  fi

fi

if [ "x$TRAVIS_GNU_VERSION" != "x" ]; then

  if [ "$CC" = "gcc" ]; then
    CC=gcc-$TRAVIS_GNU_VERSION
  fi

  if [ "$CXX" = "g++" ]; then
    CXX=g++-$TRAVIS_GNU_VERSION
  fi
fi
