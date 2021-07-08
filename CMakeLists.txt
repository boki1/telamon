cmake_minimum_required(VERSION 3.19.2)
project(telamon VERSION 0.01)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -fconcepts-diagnostics-depth=3")

enable_testing()

add_subdirectory(src)
