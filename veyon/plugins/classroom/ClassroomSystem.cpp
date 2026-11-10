/*
 * ClassroomSystem.cpp - UWF, audio, InternetGuard and software deployment helpers
 *
 * This file is part of the Veyon classroom modification.
 */

#include <QtGlobal>

#include <QFileInfo>
#include <QLibrary>
#include <QProcess>
#include <QStringList>

#include "ClassroomSystem.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#endif


static QString s_lastError;


[[maybe_unused]] static bool runProgram( const QString& program, const QStringList& arguments,
										QByteArray* output = nullptr )
{
	QProcess process;
	process.start( program, arguments );
	if( process.waitForFinished( 15000 ) == false )
	{
		s_lastError = process.errorString();
		return false;
	}
	if( output ) { *output = process.readAllStandardOutput() + process.readAllStandardError(); }
	if( process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 )
	{
		s_lastError = QStringLiteral("%1 exited with code %2").arg( program ).arg( process.exitCode() );
		return false;
	}
	return true;
}


QString ClassroomSystem::lastError()
{
	return s_lastError;
}


ClassroomSystem::UwfState ClassroomSystem::uwfState()
{
	UwfState state;
#ifdef Q_OS_WIN
	QByteArray output;
	if( runProgram( QStringLiteral("uwfmgr.exe"), { QStringLiteral("get-config") }, &output ) == false )
	{
		state.raw = s_lastError;
		return state;
	}
	const QString text = QString::fromLocal8Bit( output ).toLower();
	state.raw = QString::fromLocal8Bit( output );
	state.supported = text.contains( QStringLiteral("filter") ) || text.contains( QStringLiteral("\u7b5b\u9009") );
	if( state.supported == false ) { return state; }

	const int filterIndex = qMax( text.indexOf( QStringLiteral("filter") ), text.indexOf( QStringLiteral("\u7b5b\u9009") ) );
	const QString filterSection = text.mid( filterIndex, 260 );
	const bool disabled = filterSection.contains( QStringLiteral("disabled") ) ||
						  filterSection.contains( QStringLiteral("\u7981\u7528") );
	const bool enabled = filterSection.contains( QStringLiteral("enabled") ) ||
						 filterSection.contains( QStringLiteral("\u542f\u7528") );
	state.currentEnabled = enabled && disabled == false;
	state.nextEnabled = filterSection.contains( QStringLiteral("next") ) && enabled;
	state.pendingReboot = state.currentEnabled != state.nextEnabled;
#else
	state.raw = QStringLiteral("UWF is only available on Windows Enterprise/Education/IoT");
#endif
	return state;
}


bool ClassroomSystem::setUwfFilter( bool enabled )
{
#ifdef Q_OS_WIN
	return runProgram( QStringLiteral("uwfmgr.exe"),
					   { QStringLiteral("filter"), enabled ? QStringLiteral("enable") : QStringLiteral("disable") } );
#else
	Q_UNUSED(enabled)
	return false;
#endif
}


bool ClassroomSystem::restartSystem()
{
#ifdef Q_OS_WIN
	return runProgram( QStringLiteral("shutdown.exe"),
					   { QStringLiteral("/r"), QStringLiteral("/t"), QStringLiteral("5"), QStringLiteral("/f") } );
#else
	return false;
#endif
}


bool ClassroomSystem::setMute( bool muted )
{
#ifdef Q_OS_WIN
	using CoInitializeExFn = HRESULT (WINAPI*)(LPVOID, DWORD);
	using CoUninitializeFn = void (WINAPI*)();
	using CoCreateInstanceFn = HRESULT (WINAPI*)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);

	QLibrary ole32( QStringLiteral("ole32.dll") );
	if( ole32.load() == false ) { s_lastError = ole32.errorString(); return false; }
	const auto coInit = reinterpret_cast<CoInitializeExFn>( ole32.resolve( "CoInitializeEx" ) );
	const auto coUninit = reinterpret_cast<CoUninitializeFn>( ole32.resolve( "CoUninitialize" ) );
	const auto coCreate = reinterpret_cast<CoCreateInstanceFn>( ole32.resolve( "CoCreateInstance" ) );
	if( coInit == nullptr || coUninit == nullptr || coCreate == nullptr ) { return false; }

	const HRESULT initResult = coInit( nullptr, COINIT_APARTMENTTHREADED );
	if( FAILED( initResult ) && initResult != S_FALSE ) { return false; }

	static const GUID clsidEnumerator = { 0xbcde0395, 0xe52f, 0x467c,
										  { 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e } };
	static const GUID iidEnumerator = { 0xa95664d2, 0x9614, 0x4f35,
										{ 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
	static const GUID iidEndpointVolume = { 0x5cdf2c82, 0x841e, 0x4546,
											{ 0x97, 0x22, 0x0c, 0xf7, 0x40, 0x78, 0x22, 0x9 } };

	IMMDeviceEnumerator* enumerator = nullptr;
	IMMDevice* device = nullptr;
	IAudioEndpointVolume* volume = nullptr;
	bool ok = false;

	if( SUCCEEDED( coCreate( clsidEnumerator, nullptr, CLSCTX_ALL, iidEnumerator,
							 reinterpret_cast<void**>( &enumerator ) ) ) && enumerator &&
		SUCCEEDED( enumerator->GetDefaultAudioEndpoint( eRender, eMultimedia, &device ) ) && device &&
		SUCCEEDED( device->Activate( iidEndpointVolume, CLSCTX_ALL, nullptr,
									 reinterpret_cast<void**>( &volume ) ) ) && volume )
	{
		ok = SUCCEEDED( volume->SetMute( muted ? TRUE : FALSE, nullptr ) );
	}
	else
	{
		s_lastError = QStringLiteral("Could not open the default audio endpoint");
	}

	if( volume ) { volume->Release(); }
	if( device ) { device->Release(); }
	if( enumerator ) { enumerator->Release(); }
	if( SUCCEEDED( initResult ) || initResult == S_FALSE ) { coUninit(); }
	return ok;
#else
	Q_UNUSED(muted)
	return false;
#endif
}


bool ClassroomSystem::setInternetBlock( bool blocked, const QString& teacherIp )
{
#ifdef Q_OS_WIN
	// Windows Firewall is the user-mode configuration interface of WFP. Using the
	// signed netsh binary keeps Defender happy and needs no driver or extra library.
	if( blocked )
	{
		if( runProgram( QStringLiteral("netsh.exe"),
						{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
						  QStringLiteral("rule"), QStringLiteral("name=ClassroomInternetGuard"),
						  QStringLiteral("dir=out"), QStringLiteral("action=block") } ) == false )
		{
			return false;
		}
		if( teacherIp.isEmpty() == false )
		{
			runProgram( QStringLiteral("netsh.exe"),
						{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
						  QStringLiteral("rule"), QStringLiteral("name=ClassroomInternetGuardAllow"),
						  QStringLiteral("dir=out"), QStringLiteral("action=allow"),
						  QStringLiteral("remoteip=%1").arg( teacherIp ) } );
		}
		return true;
	}

	const bool removed = runProgram( QStringLiteral("netsh.exe"),
									 { QStringLiteral("advfirewall"), QStringLiteral("firewall"),
									   QStringLiteral("delete"), QStringLiteral("rule"),
									   QStringLiteral("name=ClassroomInternetGuard") } );
	runProgram( QStringLiteral("netsh.exe"),
				{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
				  QStringLiteral("rule"), QStringLiteral("name=ClassroomInternetGuardAllow") } );
	return removed;
#else
	Q_UNUSED(blocked)
	Q_UNUSED(teacherIp)
	return false;
#endif
}


QString ClassroomSystem::softwareInstallCommand( const QString& packagePath )
{
	const auto lower = packagePath.toLower();
	if( lower.endsWith( QStringLiteral(".msi") ) )
	{
		return QStringLiteral("msiexec.exe /i \"%1\" /quiet /norestart").arg( packagePath );
	}
	if( lower.endsWith( QStringLiteral(".bat") ) || lower.endsWith( QStringLiteral(".cmd") ) )
	{
		return QStringLiteral("cmd.exe /c \"%1\"").arg( packagePath );
	}
	// Inno Setup / NSIS / InstallShield all accept /VERYSILENT or /S; the teacher
	// can override the arguments in the deployment dialog.
	return QStringLiteral("\"%1\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /S").arg( packagePath );
}
