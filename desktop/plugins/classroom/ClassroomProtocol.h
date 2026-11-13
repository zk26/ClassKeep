/*
 * ClassroomProtocol.h - feature UIDs, commands and arguments shared by all components
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include <QAbstractSocket>
#include <QHostAddress>
#include <QString>
#include <QUuid>

#include "Feature.h"
#include "FeatureMessage.h"

namespace Classroom
{

// Visible product name. Internal identifiers (config keys, service names, data
// directories, wire protocol) intentionally stay lowercase "ikeback".
inline QString productName()
{
	return QStringLiteral("iKeback");
}

inline QString productNameEn()
{
	return QStringLiteral("iKeback");
}

/** UDP announcement shared with the ikeback mobile app. The lesson key is NEVER included. */
inline QString announcementPrefix()
{
	return QStringLiteral("IKEBACK:");
}

inline QString discoveryMagic()
{
	return QStringLiteral("IKEBACK_DISCOVER");
}

//! Validate anything that ends up in "remoteip=" (netsh) or in a listen()
//! call. CIDR ranges, wildcards, loopback, multicast and broadcast are
//! rejected. Returns a null QHostAddress for invalid input.
inline QHostAddress unicastAddress( const QString& value )
{
	QHostAddress address;
	const auto text = value.trimmed();
	if( text.isEmpty() || address.setAddress( text ) == false ||
		address.protocol() != QAbstractSocket::IPv4Protocol )
	{
		return {};
	}
	const auto ipv4 = address.toIPv4Address();
	if( ipv4 == 0 || ipv4 == 0xffffffffu || address.isLoopback() || address.isMulticast() ||
		address == QHostAddress( QHostAddress::Any ) )
	{
		return {};
	}
	return address;
}

inline QString sanitizeUnicastIp( const QString& value )
{
	const auto address = unicastAddress( value );
	return address.isNull() ? QString{} : address.toString();
}

inline Feature::Uid pluginUid()
{
	return Feature::Uid{ QStringLiteral("d3f1c0de-7a41-4b8e-9c12-000000000000") };
}

inline Feature::Uid consoleFeatureUid()
{
	return Feature::Uid{ QStringLiteral("d3f1c0de-7a41-4b8e-9c12-000000000001") };
}

inline Feature::Uid clientFeatureUid()
{
	return Feature::Uid{ QStringLiteral("d3f1c0de-7a41-4b8e-9c12-000000000002") };
}

//! 5.2.0: ikeback's own name+seat sign-in replaces Ikeback's username/password
//! logon in the Master tool bar. These are Action|Master features; the messages
//! they send still travel over clientFeatureUid() so the wire format is unchanged.
inline Feature::Uid signInFeatureUid()
{
	return Feature::Uid{ QStringLiteral("d3f1c0de-7a41-4b8e-9c12-000000000003") };
}

inline Feature::Uid signOutFeatureUid()
{
	return Feature::Uid{ QStringLiteral("d3f1c0de-7a41-4b8e-9c12-000000000004") };
}

//! 5.2.0: quick tools live on the Ikeback Master tool bar (drop-down menu) and
//! attendance check-in gets its own button. These are Action|Master features:
//! they only open UI, so they never start another session worker.
inline Feature::Uid quickToolsFeatureUid()
{
	return Feature::Uid{ QStringLiteral("d3f1c0de-7a41-4b8e-9c12-000000000005") };
}

inline Feature::Uid checkInFeatureUid()
{
	return Feature::Uid{ QStringLiteral("d3f1c0de-7a41-4b8e-9c12-000000000006") };
}

enum class Command
{
	StartClient = 1,
	StopClient,
	SetSettings,
	SetRoster,
	SetSessionKey,
	SetIdentity,
	ClearIdentity,
	ForceLogout,
	SwitchStudent,
	LockClient,
	UnlockClient,
	ToggleAi,
	ReportEvent,
	ReportIdentity,
	RequestQuit,
	Ping,

	// scoring
	SetScoreRules,
	QueryScore,
	ScoreReply,

	// classroom interaction
	CallTeacher,
	DismissCall,
	ShowSilencePage,
	HideSilencePage,

	// device and network control
	SetMute,
	SetInternetBlock,
	SetUsbInputBlock,
	StartRemoteControl,
	StartGroupDemo,
	StartMultiDemo,

	// UWF and software deployment
	UwfQuery,
	UwfSetFilter,
	UwfRestart,
	UwfInstall,
	UwfStatusReport,

	// teacher machine <-> mobile bridge
	SyncRequest,
	SyncReply,

	// attendance check-in (appended last: numeric values are the wire protocol)
	StartCheckIn,
	CheckInReply,

	// student uninstall protection (ALWAYS append: values are the wire protocol)
	// Argument::Enabled carries on/off.
	SetUninstallProtection,

	// 5.2.0 teacher quick tools. ALWAYS append: numeric values are the wire
	// protocol and must stay compatible with already deployed 1.x workers.
	// "Protection" and "UWF" are TWO INDEPENDENT switches:
	//   SetExitProtection = client process protection (student cannot quit the
	//                       ikeback worker / floating card).
	//   UwfSetFilter      = OS-level write filter / restore-on-reboot, used to
	//                       survive reboots and to install software.
	SetExitProtection,
	OpenSoftware,
	OpenWebsite,
	SetAppBlockRules,
	SetWebBlockRules,
	ProtectionStateReport,

	// 5.2.0 worker consolidation. ALWAYS append (numeric values are the wire
	// protocol). One ApplySession message replaces the old SetSettings +
	// SetRoster + SetSessionKey burst, which used to re-arm the session worker
	// restart logic three times every time the teacher opened the console.
	ApplySession,

	// 5.2.0: students no longer hold the AI API key. They submit a prompt and
	// the teacher machine calls the model provider on their behalf.
	AiPrompt,
	AiReply
};

enum class Argument
{
	SettingsJson,
	RosterJson,
	SessionKey,
	ClassName,
	Seat,
	Student,
	SessionId,
	Host,
	Ip,
	EventType,
	Category,
	Detail,
	App,
	Timestamp,
	Enabled,
	Reason,
	NewSeat,
	NewStudent,

	// scoring
	ScoreJson,
	RulesJson,
	LessonId,
	LessonTopic,
	LessonDate,
	Delta,
	Label,
	RichText,
	ImageRefs,
	Author,
	BaseScore,
	Published,
	TotalScore,
	EntryId,

	// interaction
	CallId,
	Message,
	GroupId,
	Mode,
	TargetIps,

	// UWF / deployment
	SoftwarePath,
	SoftwareArgs,
	InstallerType,
	UwfStateJson,
	Progress,
	Result,

	// bridge / sync
	Revision,
	SyncDataJson,
	TeacherId,
	BridgePort,
	DeviceClass,
	BlockMinutes,

	// attendance check-in
	CheckInName,

	// 5.2.0: student machines receive only this boolean, never the key itself.
	// SetSessionKey / StartClient / ApplySession use it in place of SessionKey.
	KeyConfigured,

	// teacher-machine AI proxy (prompt JSON, reply)
	AiPayload,
	AiSuccess,
	AiAnswer
};

inline FeatureMessage message( Command command )
{
	return FeatureMessage{ clientFeatureUid(), command };
}

namespace EventType
{
inline QString login()        { return QStringLiteral("login"); }
inline QString logout()       { return QStringLiteral("logout"); }
inline QString switchUser()   { return QStringLiteral("switch"); }
inline QString assign()       { return QStringLiteral("assign"); }
inline QString app()          { return QStringLiteral("app"); }
inline QString window()       { return QStringLiteral("window"); }
inline QString web()          { return QStringLiteral("web"); }
inline QString ai()           { return QStringLiteral("ai"); }
inline QString lock()         { return QStringLiteral("lock"); }
inline QString unlock()       { return QStringLiteral("unlock"); }
inline QString keyFailed()    { return QStringLiteral("key-failed"); }
inline QString quitAttempt()  { return QStringLiteral("quit-attempt"); }
inline QString quitAllowed()  { return QStringLiteral("quit-allowed"); }
inline QString connection()   { return QStringLiteral("connection"); }
inline QString score()        { return QStringLiteral("score"); }
inline QString call()         { return QStringLiteral("call"); }
inline QString silence()      { return QStringLiteral("silence"); }
inline QString mute()         { return QStringLiteral("mute"); }
inline QString internet()     { return QStringLiteral("internet"); }
inline QString usb()          { return QStringLiteral("usb"); }
inline QString uwf()          { return QStringLiteral("uwf"); }
inline QString install()      { return QStringLiteral("install"); }
inline QString sync()         { return QStringLiteral("sync"); }
inline QString remote()       { return QStringLiteral("remote"); }
inline QString demo()         { return QStringLiteral("demo"); }
inline QString checkIn()      { return QStringLiteral("check-in"); }
inline QString quickScore()   { return QStringLiteral("quick-score"); }
inline QString protection()   { return QStringLiteral("protection"); }
inline QString appBlock()     { return QStringLiteral("app-block"); }
inline QString webBlock()     { return QStringLiteral("web-block"); }
inline QString software()     { return QStringLiteral("software"); }
inline QString website()      { return QStringLiteral("website"); }
}

namespace SettingKey
{
inline QString enabled()           { return QStringLiteral("enabled"); }
inline QString aiEnabled()         { return QStringLiteral("aiEnabled"); }
inline QString aiEndpoint()        { return QStringLiteral("aiEndpoint"); }
inline QString aiApiKey()          { return QStringLiteral("aiApiKey"); }
inline QString aiModel()           { return QStringLiteral("aiModel"); }
inline QString aiSystemPrompt()    { return QStringLiteral("aiSystemPrompt"); }
inline QString aiTimeout()         { return QStringLiteral("aiTimeout"); }
inline QString monitoringEnabled() { return QStringLiteral("monitoringEnabled"); }
inline QString captureInterval()   { return QStringLiteral("captureInterval"); }
inline QString captureWindowTitle(){ return QStringLiteral("captureWindowTitle"); }
inline QString captureProcess()    { return QStringLiteral("captureProcess"); }
inline QString exitProtection()    { return QStringLiteral("exitProtection"); }
inline QString exitRequiresAdmin() { return QStringLiteral("exitRequiresAdmin"); }
inline QString exitAdminUsername() { return QStringLiteral("exitAdminUsername"); }
inline QString allowSelfLogin()    { return QStringLiteral("allowSelfLogin"); }
inline QString dataDirectory()     { return QStringLiteral("dataDirectory"); }
inline QString scoreBaseScore()    { return QStringLiteral("scoreBaseScore"); }
inline QString uwfEnabled()         { return QStringLiteral("uwfEnabled"); }
inline QString internetGuard()      { return QStringLiteral("internetGuard"); }
inline QString usbInputBlock()      { return QStringLiteral("usbInputBlock"); }
inline QString bridgePort()         { return QStringLiteral("bridgePort"); }
inline QString bridgeEnabled()      { return QStringLiteral("bridgeEnabled"); }
//! Boolean only: tells a student worker that the teacher has started a lesson.
inline QString keyConfigured()      { return QStringLiteral("keyConfigured"); }
inline QString syncEnabled()        { return QStringLiteral("syncEnabled"); }
inline QString silencePageText()    { return QStringLiteral("silencePageText"); }
inline QString wordCloudStopWords() { return QStringLiteral("wordCloudStopWords"); }
//! Fuzzy (case-insensitive substring) rules persisted on the teacher machine.
inline QString blockedApps()        { return QStringLiteral("blockedApps"); }
inline QString blockedWebsites()    { return QStringLiteral("blockedWebsites"); }
}

}
