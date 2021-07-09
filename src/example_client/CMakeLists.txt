add_library(linked_list list/LinkedList.hh)

set_target_properties(linked_list PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(linked_list PRIVATE expected_lite)
target_include_directories(linked_list PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
