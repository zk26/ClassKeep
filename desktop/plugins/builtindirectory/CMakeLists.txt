include(BuildIkebackPlugin)

build_ikeback_plugin(builtindirectory
	NAME BuiltinDirectory
	SOURCES
	BuiltinDirectoryPlugin.cpp
	BuiltinDirectoryConfigurationPage.cpp
	BuiltinDirectoryConfigurationPage.ui
	BuiltinDirectory.cpp
	BuiltinDirectoryPlugin.h
	BuiltinDirectoryConfiguration.h
	BuiltinDirectoryConfigurationPage.h
	BuiltinDirectory.h
	builtindirectory.qrc
	)
