include(BuildIkebackPlugin)

build_ikeback_plugin(powercontrol
	NAME PowerControl
	SOURCES
	PowerControlFeaturePlugin.cpp
	PowerControlFeaturePlugin.h
	PowerDownTimeInputDialog.cpp
	PowerDownTimeInputDialog.h
	PowerDownTimeInputDialog.ui
	powercontrol.qrc
	)
