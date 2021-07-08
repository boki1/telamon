add_compile_definitions(TEL_LOGGING)

add_library(
        telamon
        HelpQueue.hh
		NormalizedRepresentation.hh
        OperationHelping.hh WaitFreeSimulator.hh Versioning.hh)

set_target_properties(telamon PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(telamon PRIVATE loguru)
target_link_libraries(telamon PRIVATE expected_lite)
