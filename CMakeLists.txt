cmake_minimum_required (VERSION 2.8.11)

if (NOT CMAKE_BUILD_TYPE MATCHES "")
	message(STATUS "Configuring in ${CMAKE_BUILD_TYPE} mode")
endif ()

project (leviathan)

include (GNUInstallDirs)

option(STATIC_LIBS "Enable static linking" OFF)
option(ENABLE_TESTS "Enable test suite" ON)

if(STATIC_LIBS)
  set(BUILD_SHARED_LIBS OFF)
  message("-- Linking a static project")
else()
  set(BUILD_SHARED_LIBS ON)
endif()

if(CMAKE_GENERATOR MATCHES "Xcode")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot /")
endif ()

add_subdirectory (external/cppformat)
add_subdirectory (external/tclap)
add_subdirectory (external/optional)
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
