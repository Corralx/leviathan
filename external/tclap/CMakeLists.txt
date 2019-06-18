cmake_minimum_required (VERSION 2.8.11)
project (tclap CXX)

add_library(tclap INTERFACE)
target_include_directories(tclap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
