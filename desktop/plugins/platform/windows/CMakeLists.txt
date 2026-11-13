include(BuildIkebackPlugin)

set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)

build_ikeback_plugin(windows-platform
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
	authSSP.cpp

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
	${ultravnc_DIR}/winvnc/winvnc
	${ultravnc_DIR}
)

# ikeback: upstream links -linterception, which only exists in Ikeback's private
# cross-build image (not in MSYS2, not in 3rdparty/interception/). The optional
# driver API is stubbed in 3rdparty/interception/include/interception.h so the
# mandatory windows-platform plugin can be built. See that header for details.
target_link_libraries(windows-platform PRIVATE -lws2_32 -lwtsapi32 -lnetapi32 -luserenv -limm32 -lntdll -liphlpapi -lsetupapi -lsecur32)
target_link_libraries(windows-platform PRIVATE Qt6::GuiPrivate)
# ikeback: do NOT redefine _WIN32_WINNT / WINVER here. The MSYS2 UCRT64
# toolchain already defines them, and Ikeback's global -Werror turns the
# "redefined" warning into a hard error. Fixing the windows.h include order
# in the .cpp files is sufficient for DsGetDcNameW / WTSQueryUserToken /
# AF_INET6 / ConvertSidToStringSidW to be visible.
target_compile_definitions(windows-platform PRIVATE ULTRAVNC_IKEBACK_SUPPORT)
# ikeback: this target also compiles the pinned upstream UltraVNC sources
# (vnckeymap.cpp / authSSP.cpp). Modern GCC + Ikeback's global -Wall -Werror
# flag their legacy unused variables as hard errors. Scope the exemptions to
# this target only; ikeback's own code keeps full warnings-as-errors.
target_compile_options(windows-platform PRIVATE
	"-Wno-unknown-pragmas;-flarge-source-files;-Wno-misleading-indentation;-Wno-unused-but-set-variable;-Wno-unused-variable;-Wno-deprecated-declarations;-Wno-sign-compare;-Wno-unused-result;-Wno-error=cpp"
)
set_source_files_properties(WindowsNetworkFunctions.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE SKIP_PRECOMPILE_HEADERS TRUE)

# NOTE: vnckeymap.cpp comes from the pinned UltraVNC tree and its VNCLOG() macro
# relies on __FUNCTION__ being a string literal. Current GCC makes it a pointer,
# so the macro no longer compiles. The CI fetch step patches that single macro
# (see .github/workflows/ikeback-windows.yml) instead of forking the sources.
#
# Its SettingsManager.h is ANSI-only: under the project-wide UNICODE/_UNICODE
# defines TCHAR becomes wchar_t and the legacy char* accessors no longer
# compile. CMake offers no per-source way to cancel those two defines (its
# COMPILE_DEFINITIONS channel would emit -D-UUNICODE), so the CI fetch step
# prepends #undef UNICODE/_UNICODE to vnckeymap.cpp - before any include, which
# is the only point where it still has an effect. See
# .github/workflows/ikeback-windows.yml.
