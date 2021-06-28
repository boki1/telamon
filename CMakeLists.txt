cmake_minimum_required(VERSION 3.20.0)
project(telamon VERSION 0.01)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

enable_testing()

add_subdirectory(src)
