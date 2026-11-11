/*
 * ClassroomProtocol.h - feature UIDs, commands and arguments shared by all components
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QString>
#include <QUuid>

#include "Feature.h"
#include "FeatureMessage.h"

namespace Classroom
{

inline QString productName()
{
	return QStringLiteral("课守 ClassKeep");
}

inline QString productNameEn()
{
	return QStringLiteral("ClassKeep");
}

/** UDP announcement shared with the ClassKeep mobile app. The lesson key is NEVER included. */
inline QString announcementPrefix()
{
	return QStringLiteral("CLASSKEEP:");
}

inline QString discoveryMagic()
{
	return QStringLiteral("CLASSKEEP_DISCOVER");
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
	SetUninstallProtection
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
	CheckInName
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
inline QString syncEnabled()        { return QStringLiteral("syncEnabled"); }
inline QString silencePageText()    { return QStringLiteral("silencePageText"); }
inline QString wordCloudStopWords() { return QStringLiteral("wordCloudStopWords"); }
}

}
