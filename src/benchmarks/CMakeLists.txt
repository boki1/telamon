add_executable(lf-bench LockFree.cc)
target_link_libraries(lf-bench PRIVATE lockfree_list)

add_executable(wf-bench WaitFree.cc)
target_link_libraries(wf-bench PRIVATE waitfree_list)