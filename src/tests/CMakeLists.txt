FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/355d57d90d9744c41ac7c99f1e960778f1c63040.zip
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Example test
add_executable(it_works_test it_works.cc)
target_link_libraries(it_works_test gtest_main)
gtest_discover_tests(it_works_test)
#

# Help queue test
add_executable(helpqueue_test TestHelpQueue.cc TestVersioning.cc)
target_link_libraries(helpqueue_test gtest_main)
target_link_libraries(helpqueue_test telamon)
gtest_discover_tests(helpqueue_test)
#

# Versioning test
add_executable(versioning_test TestVersioning.cc)
target_link_libraries(versioning_test gtest_main)
target_link_libraries(versioning_test telamon)
gtest_discover_tests(versioning_test)

# Simulator test
add_executable(simulator_test TestSimulator.cc)
target_link_libraries(simulator_test gtest_main)
target_link_libraries(simulator_test telamon)
gtest_discover_tests(simulator_test)

# Linked list test
add_executable(linked_list_test TestLinkedList.cc)
target_link_libraries(linked_list_test gtest_main)
target_link_libraries(linked_list_test linked_list)
gtest_discover_tests(linked_list_test)
