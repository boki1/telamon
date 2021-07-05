# Loguro
add_library(
        loguru
        loguru/loguru.cpp
        loguru/loguru.hpp
)

target_link_libraries(loguru PRIVATE pthread)
target_link_libraries(loguru PRIVATE dl)
set_target_properties(loguru PROPERTIES LINKER_LANGUAGE CXX)

#Expected-lite
add_library(
	expected_lite
	expected_lite/expected.hpp
	)

set_target_properties(expected_lite PROPERTIES LINKER_LANGUAGE CXX)
