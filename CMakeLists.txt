cmake_minimum_required(VERSION 3.19.2)
project(telamon VERSION 0.01)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a")

enable_testing()

add_subdirectory(src)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)


target_link_libraries(telamon PRIVATE CONAN_PKG::expected-lite)
