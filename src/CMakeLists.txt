add_subdirectory(extern)
target_include_directories(expected_lite PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/extern")
target_include_directories(loguru PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/extern")

add_subdirectory(telamon)
target_include_directories(telamon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(example_client)
target_include_directories(lockfree_list PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(waitfree_list PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(tests)

add_subdirectory(benchmarks)
