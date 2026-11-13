/*
 * ClassroomConfiguration.h - settings for the classroom extension
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include "IkebackConfiguration.h"
#include "Configuration/Proxy.h"

#define FOREACH_CLASSROOM_CONFIG_PROPERTY(OP) \
	OP( ClassroomConfiguration, m_configuration, bool, enabled, setEnabled, "Enabled", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, dataDirectory, setDataDirectory, "DataDirectory", "Classroom", QStringLiteral("%APPDATA%/ikeback"), Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, sessionKey, setSessionKey, "SessionKey", "Classroom", QString(), Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, exitProtection, setExitProtection, "ExitProtection", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, exitRequiresAdmin, setExitRequiresAdmin, "ExitRequiresAdmin", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, exitAdminUsername, setExitAdminUsername, "ExitAdminUsername", "Classroom", QStringLiteral("Administrator"), Configuration::Property::Flag::Advanced ) \
	OP( ClassroomConfiguration, m_configuration, bool, allowSelfLogin, setAllowSelfLogin, "AllowSelfLogin", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, aiEnabled, setAiEnabled, "AiEnabled", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, aiEndpoint, setAiEndpoint, "AiEndpoint", "Classroom", QStringLiteral("http://127.0.0.1:11434/v1/chat/completions"), Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, aiApiKey, setAiApiKey, "AiApiKey", "Classroom", QString(), Configuration::Property::Flag::Advanced ) \
	OP( ClassroomConfiguration, m_configuration, QString, aiModel, setAiModel, "AiModel", "Classroom", QStringLiteral("qwen2.5:7b"), Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, aiSystemPrompt, setAiSystemPrompt, "AiSystemPrompt", "Classroom", QString(), Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, int, aiTimeout, setAiTimeout, "AiTimeout", "Classroom", 60, Configuration::Property::Flag::Advanced ) \
	OP( ClassroomConfiguration, m_configuration, bool, monitoringEnabled, setMonitoringEnabled, "MonitoringEnabled", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, int, captureInterval, setCaptureInterval, "CaptureInterval", "Classroom", 5, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, captureWindowTitle, setCaptureWindowTitle, "CaptureWindowTitle", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, captureProcess, setCaptureProcess, "CaptureProcess", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, interestCategories, setInterestCategories, "InterestCategories", "Classroom", QString(), Configuration::Property::Flag::Advanced ) \
	OP( ClassroomConfiguration, m_configuration, int, scoreBaseScore, setScoreBaseScore, "ScoreBaseScore", "Classroom", 100, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, uwfEnabled, setUwfEnabled, "UwfEnabled", "Classroom", false, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, internetGuard, setInternetGuard, "InternetGuard", "Classroom", false, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, usbInputBlock, setUsbInputBlock, "UsbInputBlock", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, int, bridgePort, setBridgePort, "BridgePort", "Classroom", 11880, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, bridgeEnabled, setBridgeEnabled, "BridgeEnabled", "Classroom", false, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, bool, syncEnabled, setSyncEnabled, "SyncEnabled", "Classroom", true, Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, silencePageText, setSilencePageText, "SilencePageText", "Classroom", QStringLiteral("Please keep quiet"), Configuration::Property::Flag::Standard ) \
	OP( ClassroomConfiguration, m_configuration, QString, wordCloudStopWords, setWordCloudStopWords, "WordCloudStopWords", "Classroom", QString(), Configuration::Property::Flag::Advanced ) \
	OP( ClassroomConfiguration, m_configuration, int, retentionDays, setRetentionDays, "RetentionDays", "Classroom", 180, Configuration::Property::Flag::Advanced )

DECLARE_CONFIG_PROXY(ClassroomConfiguration, FOREACH_CLASSROOM_CONFIG_PROPERTY)
