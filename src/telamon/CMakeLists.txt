add_library(
	telamon
	HelpQueue.hh
	)

target_include_directories(telamon PUBLIC "../${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(telamon PRIVATE folly)
set_target_properties(telamon PROPERTIES LINKER_LANGUAGE CXX)

