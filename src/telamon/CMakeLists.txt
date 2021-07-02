add_compile_definitions(TEL_LOGGING)

add_library(
        telamon
        HelpQueue.hh
)

set_target_properties(telamon PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(telamon PRIVATE loguru)
