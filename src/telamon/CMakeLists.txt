add_library(
        telamon
        HelpQueue.hh
        HelpQueue.cc
)

FetchContent_Declare(GSL
        GIT_REPOSITORY "https://github.com/microsoft/GSL"
        GIT_TAG "v3.1.0"
        )
FetchContent_MakeAvailable(GSL)

set_target_properties(telamon PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(telamon PRIVATE GSL)
