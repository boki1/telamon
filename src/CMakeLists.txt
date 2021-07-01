add_subdirectory(telamon)
target_include_directories(telamon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(tests)
