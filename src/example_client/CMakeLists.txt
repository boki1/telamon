add_library(linked_list list/NormalizedLinkedList.hh list/LockFreeLinkedList.hh)

set_target_properties(linked_list PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(linked_list PRIVATE expected_lite)
target_link_libraries(linked_list PRIVATE telamon)

