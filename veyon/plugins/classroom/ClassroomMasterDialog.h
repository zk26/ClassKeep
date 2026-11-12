/*
 * ClassroomMasterDialog.h - teacher console for classes, logs, analysis and AI
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QDialog>
#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVector>

#include "ClassroomData.h"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QTimer;
class ClassroomAiClient;
class ClassroomBridge;
class ClassroomScorePanel;
class ClassroomSeatGrid;
class ComputerControlInterface;
class ClassroomConfiguration;
class ClassroomLogStore;
class ClassroomRoster;
class ClassroomSettings;
class FeatureMessage;
class VeyonMasterInterface;

class ClassroomMasterDialog : public QDialog
{
	Q_OBJECT
public:
	ClassroomMasterDialog( VeyonMasterInterface& master,
						   ClassroomConfiguration& configuration,
						   ClassroomRoster& roster,
						   ClassroomLogStore& store,
						   ClassroomSettings& settings,
						   QWidget* parent = nullptr );
	~ClassroomMasterDialog() override;

	void showCall( const QString& ip, const QString& seat, const QString& student );
	void syncNow();
	//! Called by the plugin when a student submits the attendance prompt.
	void recordCheckIn( const QString& className, const QString& seat, const QString& typedName );

private:
	void setupClassTab();
	void setupLogTab();
	void setupAnalysisTab();
	void setupAiTab();
	void setupMonitorTab();
	void setupSystemTab();
	void setupSeatTab();
	void refreshSeatGrid();
	void refreshUwfStatus();
	//! 2.1.0 teacher-side parity with the phone: timetable + per-lesson scoring.
	void setupTimetableTab();
	void refreshLessonRecords();
	QString timetableFilePath() const;

	//! 2.0.0 quick tools. "Client protection" and "UWF" are separate switches.
	void setupToolsTab();
	QStringList toolTargets() const;
	void sendToTargets( const FeatureMessage& message );
	void setToolStatus( const QString& text );
	void setClientProtection( bool enabled );
	void setUwfFilter( bool enabled );
	void applyFuzzyRules( const QString& key, const QString& text );
	void openOnTargets( bool website, const QString& target );
	void scoreWholeClass( int delta );
	void startCheckIn();
	void refreshCheckIn();

	QString dataDirectory() const;
	QString currentClassName() const;
	QString currentSessionId();
	QJsonObject settingsJson() const;
	QString sessionKey() const;

	void addClass();
	void importRoster();
	void exportRoster();
	void saveRoster();
	void registerComputers( const QVector<ClassroomStudent>& students );
	void refreshRosterTable();
	void refreshStatus();
	void refreshMachines();

	void startNewSession();
	void generateKey();
	void assignByIp();
	void quickLoginByIp();
	void batchLogin();
	void batchLogout();
	void forceLogout();
	void switchStudent();
	void lockAll( bool locked );
	void toggleAi( bool enabled );
	void pushSettings();
	void pushRoster();
	void pushKey();
	void pushIdentity( const QString& ip, const QString& seat, const QString& name );
	void pushLogout( const QString& ip );
	void sendToAll( const FeatureMessage& message );
	void sendToIp( const QString& ip, const FeatureMessage& message );
	void queueDirectMessage( const QString& ip, const FeatureMessage& message, int retries );
	QStringList selectedIps() const;
	QStringList onlineIps() const;
	static QString ipOfInterface( const class ComputerControlInterface* controlInterface );
	void appendTeacherEvent( const QString& type, const QString& detail, const QString& ip = QString{},
							 const QString& seat = QString{}, const QString& student = QString{} );

	void refreshLogs();
	void openLogFolder();
	void exportLogs();
	void purgeLogs();
	QVector<ClassroomEvent> filteredEvents() const;

	void refreshAnalysis();
	void refreshSettingsWidgets();
	void saveSettingsFromWidgets();
	void editCategories();
	void exportAnalysis();
	void testAiConnection();

	VeyonMasterInterface& m_master;
	ClassroomConfiguration& m_configuration;
	ClassroomRoster& m_roster;
	ClassroomLogStore& m_store;
	ClassroomSettings& m_settings;
	ClassroomAiClient* m_testAiClient = nullptr;
	ClassroomScorePanel* m_scorePanel = nullptr;
	ClassroomBridge* m_bridge = nullptr;
	QTimer* m_refreshTimer = nullptr;

	QString m_sessionId;
	int m_refreshTicks = 0;
	QMap<QString, QString> m_status;
	QMap<QString, QSharedPointer<ComputerControlInterface>> m_directInterfaces;
	QVector<ClassroomEvent> m_logEvents;
	QVector<ClassroomInterestProfile> m_profiles;

	QTabWidget* m_tabs = nullptr;

	QComboBox* m_classCombo = nullptr;
	QTableWidget* m_rosterTable = nullptr;
	QLabel* m_keyLabel = nullptr;
	QLabel* m_sessionLabel = nullptr;
	QLineEdit* m_quickIp = nullptr;
	QLineEdit* m_quickSeat = nullptr;
	QLineEdit* m_quickName = nullptr;

	QDateEdit* m_logFrom = nullptr;
	QDateEdit* m_logTo = nullptr;
	QComboBox* m_logScope = nullptr;
	QComboBox* m_logMachine = nullptr;
	QComboBox* m_logType = nullptr;
	QLineEdit* m_logKeyword = nullptr;
	QTableWidget* m_logTable = nullptr;

	QSpinBox* m_halfLife = nullptr;
	QTableWidget* m_analysisTable = nullptr;
	QLabel* m_analysisHint = nullptr;

	QCheckBox* m_aiEnabled = nullptr;
	QLineEdit* m_aiEndpoint = nullptr;
	QLineEdit* m_aiApiKey = nullptr;
	QLineEdit* m_aiModel = nullptr;
	QPlainTextEdit* m_aiSystemPrompt = nullptr;
	QSpinBox* m_aiTimeout = nullptr;
	QLabel* m_aiTestResult = nullptr;

	QCheckBox* m_monitoringEnabled = nullptr;
	QCheckBox* m_captureWindowTitle = nullptr;
	QCheckBox* m_captureProcess = nullptr;
	QCheckBox* m_exitProtection = nullptr;
	QCheckBox* m_exitRequiresAdmin = nullptr;
	QCheckBox* m_allowSelfLogin = nullptr;
	QLineEdit* m_exitAdminUsername = nullptr;
	QSpinBox* m_captureInterval = nullptr;
	QSpinBox* m_retentionDays = nullptr;
	QLineEdit* m_dataDirectory = nullptr;

	QLabel* m_systemStatus = nullptr;
	QLineEdit* m_teacherIp = nullptr;
	QLineEdit* m_packagePath = nullptr;
	QLineEdit* m_silenceText = nullptr;
	QCheckBox* m_bridgeEnabled = nullptr;
	QSpinBox* m_bridgePort = nullptr;

	ClassroomSeatGrid* m_seatGrid = nullptr;
	QTableWidget* m_checkInTable = nullptr;

	// Quick tools tab (inserted as the first tab).
	QComboBox* m_toolScope = nullptr;
	QLabel* m_toolsStatus = nullptr;
	QLineEdit* m_toolSilence = nullptr;
	QLineEdit* m_toolAppRules = nullptr;
	QLineEdit* m_toolWebRules = nullptr;
	QLineEdit* m_toolSoftware = nullptr;
	QLineEdit* m_toolWebsite = nullptr;
	QMap<QString, QString> m_uwfStatus; //!< seat -> latest UWF report from that machine
	QLineEdit* m_lessonTopic = nullptr;
	QPlainTextEdit* m_lessonRecords = nullptr;
	QLabel* m_timetableSummary = nullptr;
	QLabel* m_checkInSummary = nullptr;
	QMap<QString, QString> m_checkInNames;
	QMap<QString, QDateTime> m_checkInTimes;
	QDateTime m_checkInStarted;
};
