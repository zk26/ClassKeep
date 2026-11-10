include(BuildVeyonPlugin)

build_veyon_plugin(classroom
	NAME Classroom
	SOURCES
	ClassroomAiClient.cpp
	ClassroomAiClient.h
	ClassroomBridge.cpp
	ClassroomBridge.h
	ClassroomClient.cpp
	ClassroomClient.h
	ClassroomConfiguration.h
	ClassroomConfigurationPage.cpp
	ClassroomConfigurationPage.h
	ClassroomData.cpp
	ClassroomData.h
	ClassroomMasterDialog.cpp
	ClassroomMasterDialog.h
	ClassroomPlugin.cpp
	ClassroomPlugin.h
	ClassroomProtocol.h
	ClassroomScore.cpp
	ClassroomScore.h
	ClassroomScorePanel.cpp
	ClassroomScorePanel.h
	ClassroomSeatGrid.cpp
	ClassroomSeatGrid.h
	ClassroomSecrets.cpp
	ClassroomSecrets.h
	ClassroomSystem.cpp
	ClassroomSystem.h
	ClassroomTermReport.cpp
	ClassroomTermReport.h
	ClassroomWordCloud.cpp
	ClassroomWordCloud.h
	)

test_veyon_plugin(classroom ClassroomSelfTest)
if(WITH_TESTS)
	target_include_directories(ClassroomSelfTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	if(WIN32)
		# The plugin does not export business classes explicitly; a test executable
		# needs them visible in the SHARED import library on Windows.
		set_property(TARGET classroom PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
	else()
		set_tests_properties(ClassroomSelfTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
	endif()
endif()
