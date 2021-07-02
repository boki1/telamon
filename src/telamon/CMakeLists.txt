add_library(
        telamon
        HelpQueue.hh
        HelpQueue.cc
)

set_target_properties(telamon PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(telamon PRIVATE loguru)
