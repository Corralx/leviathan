cmake_minimum_required (VERSION 3.10)
project (checker CXX)

# Prevent Boost library autolink
add_definitions ("-DBOOST_ALL_NO_LIB")

#
# Checker executable
#
set (SRC main.cpp)

add_executable (checker ${SRC})

target_include_directories(
  checker
  PRIVATE
    checker/
    "${CPPUTILS_HEADERS}"
    "${TCLAP_HEADERS}"
    "${STDOPTIONAL_HEADERS}"
)

if (NOT (CMAKE_SYSTEM_NAME MATCHES "Windows"))
   target_compile_definitions(checker PRIVATE HAVE_LONG_LONG)
endif ()

set(
  WARNINGS
  -Wno-global-constructors
  -Wno-exit-time-destructors
)

target_compile_options(checker PUBLIC
  "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${WARNINGS}>")

if(STATIC_LIBS MATCHES "y")
  set_target_properties(checker PROPERTIES LINK_FLAGS "-static -pthread")
endif()

set_property(TARGET checker PROPERTY OUTPUT_NAME leviathan)
set_property(TARGET checker
             PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

target_link_libraries (checker leviathan tclap)

install (TARGETS checker RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
