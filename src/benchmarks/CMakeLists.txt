cmake_minimum_required(VERSION 3.19.2)
project(telamon–benchmarks VERSION 0.01)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a")

add_executable(lf-bench LockFree_gbench.cc)
target_link_libraries(lf-bench PRIVATE lockfree_list)

add_executable(wf-bench WaitFree_gbench.cc)
target_link_libraries(wf-bench PRIVATE waitfree_list)
target_link_libraries(wf-bench PRIVATE telamon)

add_executable(lf LockFree.cc)
target_link_libraries(lf PRIVATE lockfree_list)

add_executable(wf WaitFree.cc)
target_link_libraries(wf PRIVATE waitfree_list)
target_link_libraries(wf PRIVATE telamon)
