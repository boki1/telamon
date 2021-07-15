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
