add_library(telamon
	HelpQueue.hh
	NormalizedRepresentation.hh
	OperationHelping.hh
	WaitFreeSimulator.hh
	Versioning.hh)

set_target_properties(telamon PROPERTIES LINKER_LANGUAGE CXX)
