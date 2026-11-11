include(BuildVeyonPlugin)

set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)

build_veyon_plugin(windows-platform
	NAME WindowsPlatform
	SOURCES
	WindowsPlatformPlugin.cpp
	WindowsPlatformConfigurationPage.h
	WindowsPlatformConfigurationPage.cpp
	WindowsPlatformConfigurationPage.ui
	WindowsCoreFunctions.cpp
	WindowsDeviceFunctions.cpp
	WindowsFilesystemFunctions.cpp
	WindowsInputDeviceFunctions.cpp
	WindowsKeyboardShortcutTrapper.cpp
	WindowsNetworkFunctions.cpp
	WindowsServerProcess.cpp
	WindowsServiceControl.cpp
	WindowsServiceCore.cpp
	WindowsServiceFunctions.cpp
	WindowsSessionFunctions.cpp
	WindowsUserFunctions.cpp
	WtsSessionManager.cpp
	DesktopInputController.cpp
	SasEventListener.cpp
	XEventLog.cpp
	${ultravnc_DIR}/winvnc/winvnc/vnckeymap.cpp
	${ultravnc_DIR}/addon/ms-logon/authSSP/authSSP.cpp
	${ultravnc_DIR}/addon/ms-logon/authSSP/GenClientServerContext.cpp

	WindowsPlatformConfiguration.h
	WindowsPlatformPlugin.h
	WindowsCoreFunctions.h
	WindowsDeviceFunctions.h
	WindowsFilesystemFunctions.h
	WindowsInputDeviceFunctions.h
	WindowsKeyboardShortcutTrapper.h
	WindowsNetworkFunctions.h
	WindowsServerProcess.h
	WindowsServiceControl.h
	WindowsServiceCore.h
	WindowsServiceFunctions.h
	WindowsSessionFunctions.h
	WindowsSmartObjects.h
	WindowsUserFunctions.h
	WtsSessionManager.h
	DesktopInputController.h
	SasEventListener.h
	../common/LogonHelper.h
	../common/LogonHelper.cpp
	../common/PersistentLogonCredentials.h
	../common/PersistentLogonCredentials.cpp
	../common/PlatformSessionManager.h
	../common/PlatformSessionManager.cpp
	../common/ServiceDataManager.h
	../common/ServiceDataManager.cpp
	XEventLog.h
	windows.qrc
	)

target_include_directories(windows-platform PRIVATE
	../common
	${CMAKE_SOURCE_DIR}/3rdparty/interception/include
	${ultravnc_DIR}/addon/ms-logon/authSSP
	${ultravnc_DIR}/winvnc/winvnc
	${ultravnc_DIR}
	)

# ClassKeep: upstream links -linterception, which only exists in Veyon's private
# cross-build image (not in MSYS2, not in 3rdparty/interception/). The optional
# driver API is stubbed in 3rdparty/interception/include/interception.h so the
# mandatory windows-platform plugin can be built. See that header for details.
target_link_libraries(windows-platform PRIVATE -lws2_32 -lwtsapi32 -lnetapi32 -luserenv -limm32 -lntdll -liphlpapi -lsetupapi -lsecur32)
target_link_libraries(windows-platform PRIVATE Qt6::GuiPrivate)
# ClassKeep: modern MinGW-w64 headers gate several APIs (DsGetDcNameW,
# WTSQueryUserToken, AF_INET6, ConvertSidToStringSidW) behind _WIN32_WINNT.
target_compile_definitions(windows-platform PRIVATE
	ULTRAVNC_VEYON_SUPPORT
	_WIN32_WINNT=0x0A00
	WINVER=0x0A00
)
target_compile_options(windows-platform PRIVATE "-Wno-unknown-pragmas;-flarge-source-files;-Wno-misleading-indentation")
set_source_files_properties(WindowsNetworkFunctions.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE SKIP_PRECOMPILE_HEADERS TRUE)
