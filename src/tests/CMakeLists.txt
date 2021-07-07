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
add_executable(helpqueue_test TestHelpQueue.cc)
target_link_libraries(helpqueue_test gtest_main)
target_link_libraries(helpqueue_test telamon)
gtest_discover_tests(helpqueue_test)
#

# Normalized lock free test
add_executable(normalizedrepresentation_test TestNormalizedRepresentation.cc)
target_link_libraries(normalizedrepresentation_test gtest_main)
target_link_libraries(normalizedrepresentation_test telamon)
gtest_discover_tests(normalizedrepresentation_test)
##

