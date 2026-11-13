include(BuildIkebackPlugin)

build_ikeback_plugin(external-vnc-server
	NAME ExternalVncServer
	SOURCES
	ExternalVncServer.cpp
	ExternalVncServerConfigurationWidget.cpp
	ExternalVncServerConfigurationWidget.ui
	ExternalVncServer.h
	ExternalVncServerConfiguration.h
	ExternalVncServerConfigurationWidget.h
	)
