include(BuildIkebackPlugin)

if(IKEBACK_DEBUG)
	build_ikeback_plugin(testing
		NAME Testing
		SOURCES
		TestingCommandLinePlugin.cpp
		TestingCommandLinePlugin.h
		)
endif()
