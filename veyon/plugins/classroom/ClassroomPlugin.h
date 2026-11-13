/*
 * ClassroomPlugin.h - main plugin implementing teacher console, student client
 *                     and behaviour monitoring
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <memory>

#include <QDockWidget>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include "ClassroomConfiguration.h"
#include "ClassroomMasterDialog.h"
#include "ClassroomProtocol.h"
#include "ConfigurationPagePluginInterface.h"
#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "MessageContext.h"
#include "PluginInterface.h"

class QSystemTrayIcon;
class QTimer;
class ClassroomClientWidget;
class ClassroomLogStore;
class ClassroomMonitor;
class ClassroomRoster;
class ClassroomSettings;
class VeyonServerInterface;

class ClassroomPlugin : public QObject,
						public PluginInterface,
						public FeatureProviderInterface,
						public ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.Classroom")
	Q_INTERFACES(PluginInterface FeatureProviderInterface ConfigurationPagePluginInterface)
public:
	explicit ClassroomPlugin( QObject* parent = nullptr );
	~ClassroomPlugin() override;

	Plugin::Uid uid() const override { return Classroom::pluginUid(); }
	QVersionNumber version() const override { return QVersionNumber( 1, 0 ); }
	QString name() const override { return QStringLiteral("ikeback"); }
	QString description() const override { return tr( "ikeback - classes, logs, scoring, analysis and control" ); }
	QString vendor() const override { return QStringLiteral("zk26"); }
	QString copyright() const override { return QStringLiteral("ikeback"); }

	const FeatureList& featureList() const override;

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	void startSessionFeature( VeyonServerInterface& server ) override;
	void sendAsyncFeatureMessages( VeyonServerInterface& server, const MessageContext& messageContext ) override;

	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;
	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;
	bool handleFeatureMessageFromWorker( VeyonServerInterface& server, const FeatureMessage& message ) override;
	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	ConfigurationPage* createConfigurationPage() override;

Q_SIGNALS:
	void eventReceived();

private:
	QString dataDirectory() const;
	ClassroomRoster* roster();
	ClassroomLogStore* store();
	ClassroomSettings* settings();

	void startClient( VeyonServerInterface& server );
	void registerContext( const MessageContext& context );
	bool hasActiveContext() const;
	void ensureClientUi( VeyonWorkerInterface& worker );
	void ensureTrayIcon();
	void updateTrayTooltip();
	void reportEvent( const QString& type, const QString& detail, const QString& app );
	void applyWorkerSettings( const QJsonObject& settingsObject );
	QString localIpAddress() const;

	ClassroomConfiguration m_configuration;
	const Feature m_consoleFeature;
	const Feature m_clientFeature;
	const Feature m_signInFeature;
	const Feature m_signOutFeature;
	const Feature m_quickToolsFeature;
	const Feature m_checkInFeature;
	const FeatureList m_features;

	std::unique_ptr<ClassroomRoster> m_roster;
	std::unique_ptr<ClassroomLogStore> m_store;
	std::unique_ptr<ClassroomSettings> m_settings;
	QPointer<ClassroomMasterDialog> m_dialog;
	QPointer<QDockWidget> m_consoleDock;

	VeyonServerInterface* m_server = nullptr;
	QVector<MessageContext> m_contexts;
	QVector<FeatureMessage> m_pendingEvents;
	QTimer* m_watchdog = nullptr;
	bool m_restartPending = false;
	bool m_stopAllowed = false;

	VeyonWorkerInterface* m_worker = nullptr;
	ClassroomClientWidget* m_clientWidget = nullptr;
	QSystemTrayIcon* m_trayIcon = nullptr;
	int m_trayIconRetries = 0;
	ClassroomMonitor* m_monitor = nullptr;
	QJsonObject m_workerSettings;

	QString m_hostName;
	QString m_ipAddress;
	QString m_className;
	QString m_seat;
	QString m_student;
	QString m_sessionId;
	//! Never holds the bridge key: only whether the teacher has started a lesson.
	bool m_keyConfigured = false;
};
