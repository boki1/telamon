add_library(lockfree_list list/LockFreeLinkedList.hh)
add_library(waitfree_list list/NormalizedLinkedList.hh)

set_target_properties(lockfree_list PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(lockfree_list PRIVATE pthread)
target_link_libraries(lockfree_list PRIVATE expected_lite)

set_target_properties(waitfree_list PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(waitfree_list PRIVATE expected_lite)
target_link_libraries(waitfree_list PRIVATE pthread)
