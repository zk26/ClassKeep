include(BuildIkebackPlugin)

build_ikeback_plugin(remoteaccess
	NAME RemoteAccess
	SOURCES
	RemoteAccessFeaturePlugin.h
	RemoteAccessFeaturePlugin.cpp
	RemoteAccessWidget.h
	RemoteAccessWidget.cpp
	remoteaccess.qrc)
