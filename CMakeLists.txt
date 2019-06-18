cmake_minimum_required (VERSION 3.10)

if (NOT CMAKE_BUILD_TYPE MATCHES "")
	message(STATUS "Configuring in ${CMAKE_BUILD_TYPE} mode")
endif ()

project (leviathan)

include (GNUInstallDirs)

option(STATIC_LIBS "Enable static linking" ON)
option(ENABLE_TESTS "Enable test suite" ON)

if(STATIC_LIBS)
  set(BUILD_SHARED_LIBS OFF)
  message("-- Linking with static libraries")
else()
  set(BUILD_SHARED_LIBS ON)
endif()

if(CMAKE_GENERATOR MATCHES "Xcode")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot /")
endif ()

# External libraries
option(FMT_PEDANTIC "" ON)
option(FMT_WERROR "" ON)
add_subdirectory (external/fmt)
add_subdirectory (external/tclap)

# Leviathan components
add_subdirectory (lib)
add_subdirectory (checker)

#
# Testing
#
if (ENABLE_TESTS)
  enable_testing()

  message("-- Configuring tests")

  file(STRINGS tests/tests.index TESTS)

  list(LENGTH TESTS TESTSLENGTH)
  message("-- Found ${TESTSLENGTH} tests")

  foreach(TEST IN LISTS TESTS)
    list(GET TEST 0 FILE)
    list(GET TEST 1 RESULT)

		set(TEST_NAME "${FILE}:${RESULT}")
    add_test(
      NAME "${TEST_NAME}"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMAND
        /bin/bash -c
        "test $($<TARGET_FILE:checker> --parsable '${FILE}') = '${RESULT}'"
    )

		set_tests_properties("${TEST_NAME}" PROPERTIES TIMEOUT 30)
  endforeach()
endif()
