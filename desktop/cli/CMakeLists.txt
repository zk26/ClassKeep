include(BuildIkebackApplication)

set(cli_SOURCES
	src/main.cpp
	src/ConfigCommands.cpp
	src/ConfigCommands.h
	src/FeatureCommands.cpp
	src/FeatureCommands.h
	src/PluginCommands.cpp
	src/PluginCommands.h
	src/ServiceControlCommands.cpp
	src/ServiceControlCommands.h
	src/ShellCommands.cpp
	src/ShellCommands.h
	)

build_ikeback_application(ikeback-cli
	NAME CLI
	DESCRIPTION "Ikeback Command Line Interface (console version)"
	CONSOLE
	SOURCES ${cli_SOURCES})

target_compile_options(ikeback-cli PRIVATE -Wno-date-time)

if(IKEBACK_BUILD_WINDOWS)
	build_ikeback_application(ikeback-wcli
		NAME WCLI
		DESCRIPTION "Ikeback Command Line Interface (non-console version)"
		SOURCES ${cli_SOURCES})
endif()
