cmake_minimum_required(VERSION 3.19.2)
project(telamon VERSION 0.01)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

option(RANDOM_OPTION "123")
message(RANDOM_OPTION = " ${RANDOM_OPTION}")

# Enable conan.
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# HACK, or at least a handy trick.
# For more info see b74985495f44b83f57df8a0b6eb0b8cbddacd4dd
set(CMAKE_CURRENT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

# Library core code
set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/telamon")
add_library(telamon
	${CORE_DIR}/HelpQueue.hh
	${CORE_DIR}/NormalizedRepresentation.hh
	${CORE_DIR}/OperationHelping.hh
	${CORE_DIR}/WaitFreeSimulator.hh
	${CORE_DIR}/Versioning.hh)
target_include_directories(telamon PRIVATE "${CORE_DIR}")
set_target_properties(telamon PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(telamon PRIVATE CONAN_PKG::expected-lite)

# Unit-testing code
if("${TELAMON_BUILD_TESTS}" STREQUAL "yes")
	message("-- Telamon: Adding tests to build targets.")
	set(TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
	include(GoogleTest)
	enable_testing()

	function(add_unit_test name source_file)
		message("-- Telamon: Tests: Add ${name} unit test (from ${TESTS_DIR}/${source_files})")
		set(unit_test "test_${name}")
		add_executable(${unit_test} ${TESTS_DIR}/${source_files})
		set_target_properties(${unit_test} PROPERTIES LINKER_LANGUAGE CXX)
		target_link_libraries(${unit_test} PRIVATE
			CONAN_PKG::gtest
			telamon)
		gtest_discover_tests(${unit_test})
		add_test(
			NAME ${name}
			COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/${unit_test})
	endfunction()

	add_unit_test(Helpqueue TestHelpQueue.cc)
	add_unit_test(Simulator TestSimulator.cc)
	add_unit_test(Versioning TestVersioning.cc)
endif()
