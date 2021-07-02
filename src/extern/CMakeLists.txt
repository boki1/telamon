add_library(
        loguru
        loguru/loguru.cpp
        loguru/loguru.hpp
)

target_link_libraries(loguru PRIVATE pthread)
target_link_libraries(loguru PRIVATE dl)
set_target_properties(loguru PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(loguru PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
