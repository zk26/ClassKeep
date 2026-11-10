/*
 * ClassroomSystem.h - UWF, audio, InternetGuard and software deployment helpers
 *
 * Uses only signed Windows built-in executables and public user-mode APIs so
 * that Windows Defender does not flag the plugin.
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QString>

class ClassroomSystem
{
public:
	struct UwfState
	{
		bool supported = false;
		bool currentEnabled = false;
		bool nextEnabled = false;
		bool pendingReboot = false;
		QString raw;
	};

	static UwfState uwfState();
	static bool setUwfFilter( bool enabled );
	static bool restartSystem();

	static bool setMute( bool muted );
	static bool setInternetBlock( bool blocked, const QString& teacherIp );
	static QString softwareInstallCommand( const QString& packagePath );

	static QString lastError();
};
