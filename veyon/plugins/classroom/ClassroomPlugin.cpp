/*
 * ClassroomPlugin.cpp - main plugin implementing teacher console, student client
 *                      and behaviour monitoring
 *
 * This file is part of the Veyon classroom modification.
 */

#include <QtGlobal>

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "ClassroomClient.h"
#include "ClassroomConfigurationPage.h"
#include "ClassroomData.h"
#include "ClassroomMasterDialog.h"
#include "ClassroomPlugin.h"
#include "ClassroomProtocol.h"
#include "ClassroomScore.h"
#include "ClassroomSystem.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "Filesystem.h"
#include "PlatformCoreFunctions.h"
#include "PlatformPluginInterface.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"

using namespace Classroom;

IMPLEMENT_CONFIG_PROXY(ClassroomConfiguration)


ClassroomPlugin::ClassroomPlugin( QObject* parent ) :
	QObject( parent ),
	m_configuration( &VeyonCore::config() ),
	m_consoleFeature( Feature( QStringLiteral("ClassroomConsole"),
							   Feature::Flag::Action | Feature::Flag::Master,
							   consoleFeatureUid(), Feature::Uid(),
							   Classroom::productName(), {},
							   tr( "ClassKeep - classes, sign-in by IP, logs, scoring and control" ),
							   QStringLiteral(":/core/icon22.png") ) ),
	m_clientFeature( Feature( QStringLiteral("ClassroomClient"),
							  Feature::Flag::Session | Feature::Flag::Service | Feature::Flag::Worker,
							  clientFeatureUid(), Feature::Uid(),
							  tr( "Classroom client" ), {}, {} ) ),
	m_features( { m_consoleFeature, m_clientFeature } ),
	m_hostName( QHostInfo::localHostName() )
{
	m_ipAddress = localIpAddress();
}


ClassroomPlugin::~ClassroomPlugin()
{
	delete m_clientWidget;
	m_clientWidget = nullptr;
}


QString ClassroomPlugin::localIpAddress() const
{
	for( const auto& address : QNetworkInterface::allAddresses() )
	{
		if( address.isLoopback() == false && address.protocol() == QAbstractSocket::IPv4Protocol )
		{
			return address.toString();
		}
	}
	return QHostAddress( QHostAddress::LocalHost ).toString();
}


QString ClassroomPlugin::dataDirectory() const
{
	return VeyonCore::filesystem().expandPath( m_configuration.dataDirectory() );
}


ClassroomRoster* ClassroomPlugin::roster()
{
	if( m_roster == nullptr ) { m_roster.reset( new ClassroomRoster( dataDirectory() ) ); }
	return m_roster.get();
}


ClassroomLogStore* ClassroomPlugin::store()
{
	if( m_store == nullptr ) { m_store.reset( new ClassroomLogStore( dataDirectory(), QStringLiteral("teacher") ) ); }
	return m_store.get();
}


ClassroomSettings* ClassroomPlugin::settings()
{
	if( m_settings == nullptr ) { m_settings.reset( new ClassroomSettings( dataDirectory(), m_configuration ) ); }
	return m_settings.get();
}


const FeatureList& ClassroomPlugin::featureList() const
{
	return m_features;
}


ConfigurationPage* ClassroomPlugin::createConfigurationPage()
{
	return new ClassroomConfigurationPage( m_configuration );
}


bool ClassroomPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
									  const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)
	if( featureUid != clientFeatureUid() ) { return false; }

	auto message = Classroom::message( operation == Operation::Stop ? Command::StopClient : Command::StartClient );
	if( operation != Operation::Stop )
	{
		message.addArgument( Argument::SettingsJson,
							 QString::fromUtf8( QJsonDocument( ClassroomSettings::fromConfiguration( m_configuration ) ).
												toJson( QJsonDocument::Compact ) ) );
		message.addArgument( Argument::SessionKey, m_configuration.sessionKey() );
	}
	sendFeatureMessage( message, computerControlInterfaces );
	return true;
}


bool ClassroomPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != consoleFeatureUid() ) { return false; }

	if( m_dialog.isNull() )
	{
		m_dialog = new ClassroomMasterDialog( master, m_configuration, *roster(), *store(), *settings(),
											  master.mainWindow() );
	}
	m_dialog->show();
	m_dialog->raise();
	m_dialog->activateWindow();

	auto settingsMessage = Classroom::message( Command::SetSettings );
	settingsMessage.addArgument( Argument::SettingsJson,
								 QString::fromUtf8( QJsonDocument( settings()->object() ).toJson( QJsonDocument::Compact ) ) );
	sendFeatureMessage( settingsMessage, computerControlInterfaces );

	auto rosterMessage = Classroom::message( Command::SetRoster );
	rosterMessage.addArgument( Argument::RosterJson, roster()->toJson() );
	sendFeatureMessage( rosterMessage, computerControlInterfaces );

	auto keyMessage = Classroom::message( Command::SetSessionKey );
	keyMessage.addArgument( Argument::SessionKey,
							settings()->object().value( QStringLiteral("sessionKey") ).toString() );
	keyMessage.addArgument( Argument::SessionId,
							QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") ) );
	sendFeatureMessage( keyMessage, computerControlInterfaces );

	return true;
}


void ClassroomPlugin::startClient( VeyonServerInterface& server )
{
	auto message = Classroom::message( Command::StartClient );
	message.addArgument( Argument::SettingsJson,
						 QString::fromUtf8( QJsonDocument( ClassroomSettings::fromConfiguration( m_configuration ) ).
											toJson( QJsonDocument::Compact ) ) );
	message.addArgument( Argument::SessionKey, m_configuration.sessionKey() );
	message.addArgument( Argument::SessionId,
						 QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") ) );
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
}


void ClassroomPlugin::startSessionFeature( VeyonServerInterface& server )
{
	if( VeyonCore::component() != VeyonCore::Component::Server || m_configuration.enabled() == false )
	{
		return;
	}

	m_server = &server;
	startClient( server );

	m_watchdog = new QTimer( this );
	m_watchdog->setInterval( 5000 );
	connect( m_watchdog, &QTimer::timeout, this, [this]() {
		if( m_stopAllowed || m_server == nullptr || m_configuration.enabled() == false ) { return; }
		if( m_server->featureWorkerManager().isWorkerRunning( clientFeatureUid() ) )
		{
			m_restartPending = false;
			return;
		}
		if( m_restartPending == false )
		{
			startClient( *m_server );
			m_restartPending = true;
		}
	} );
	m_watchdog->start();
}


void ClassroomPlugin::registerContext( const MessageContext& context )
{
	if( context.ioDevice() == nullptr ) { return; }
	for( auto it = m_contexts.begin(); it != m_contexts.end(); )
	{
		if( it->ioDevice() == nullptr || it->ioDevice() == context.ioDevice() )
		{
			it = m_contexts.erase( it );
		}
		else
		{
			++it;
		}
	}
	m_contexts.prepend( context );
}


bool ClassroomPlugin::hasActiveContext() const
{
	for( const auto& context : m_contexts )
	{
		if( context.ioDevice() ) { return true; }
	}
	return false;
}


void ClassroomPlugin::sendAsyncFeatureMessages( VeyonServerInterface& server, const MessageContext& messageContext )
{
	registerContext( messageContext );
	for( const auto& pending : m_pendingEvents )
	{
		for( const auto& context : m_contexts )
		{
			if( context.ioDevice() ) { server.sendFeatureMessageReply( context, pending ); }
		}
	}
	m_pendingEvents.clear();
}


bool ClassroomPlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
											const FeatureMessage& message )
{
	if( message.featureUid() != clientFeatureUid() ) { return false; }

	registerContext( messageContext );
	if( m_pendingEvents.isEmpty() == false )
	{
		for( const auto& pending : m_pendingEvents )
		{
			for( const auto& context : m_contexts )
			{
				if( context.ioDevice() ) { server.sendFeatureMessageReply( context, pending ); }
			}
		}
		m_pendingEvents.clear();
	}

	const auto command = message.command<Command>();
	if( command == Command::StopClient )
	{
		m_stopAllowed = true;
		if( m_watchdog ) { m_watchdog->stop(); }
	}
	else
	{
		m_stopAllowed = false;
		if( m_watchdog && m_watchdog->isActive() == false ) { m_watchdog->start(); }
	}

	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
	return true;
}


bool ClassroomPlugin::handleFeatureMessageFromWorker( VeyonServerInterface& server, const FeatureMessage& message )
{
	if( message.featureUid() != clientFeatureUid() ) { return false; }

	switch( message.command<Command>() )
	{
	case Command::ReportEvent:
	case Command::QueryScore:
	case Command::CallTeacher:
	case Command::CheckInReply:
	case Command::UwfStatusReport:
	case Command::ProtectionStateReport:
		if( hasActiveContext() )
		{
			for( const auto& context : m_contexts )
			{
				if( context.ioDevice() ) { server.sendFeatureMessageReply( context, message ); }
			}
		}
		else
		{
			if( m_pendingEvents.size() >= 5000 ) { m_pendingEvents.removeFirst(); }
			m_pendingEvents.append( message );
		}
		return true;

	case Command::RequestQuit:
		m_stopAllowed = true;
		if( m_watchdog ) { m_watchdog->stop(); }
		QTimer::singleShot( 0, this, [&server]() {
			server.featureWorkerManager().stopWorker( clientFeatureUid() );
		} );
		return true;

	default:
		return false;
	}
}


bool ClassroomPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
											const FeatureMessage& message )
{
	if( message.featureUid() != clientFeatureUid() ) { return false; }

	const auto command = message.command<Command>();
	if( command == Command::QueryScore )
	{
		const auto className = message.argument( Argument::ClassName ).toString();
		const auto seat = message.argument( Argument::Seat ).toString();
		const auto student = message.argument( Argument::Student ).toString();
		ClassroomScoreBook book( dataDirectory() );
		book.load( className );
		auto reply = Classroom::message( Command::ScoreReply );
		reply.addArgument( Argument::RichText, book.studentHtml( seat, student ) );
		if( computerControlInterface ) { computerControlInterface->sendFeatureMessage( reply ); }
		return true;
	}

	if( command == Command::CheckInReply )
	{
		const auto seat = message.argument( Argument::Seat ).toString();
		const auto typedName = message.argument( Argument::CheckInName ).toString();
		const auto className = message.argument( Argument::ClassName ).toString();

		ClassroomEvent event;
		event.timestamp = QDateTime::currentDateTime();
		event.className = className;
		event.seat = seat;
		event.student = message.argument( Argument::Student ).toString();
		event.ip = message.argument( Argument::Ip ).toString();
		event.host = message.argument( Argument::Host ).toString();
		event.type = EventType::checkIn();
		event.detail = typedName;
		store()->append( event );
		if( m_dialog ) { m_dialog->recordCheckIn( className, seat, typedName ); }
		Q_EMIT eventReceived();
		return true;
	}

	if( command == Command::CallTeacher )
	{
		ClassroomEvent event;
		event.timestamp = QDateTime::currentDateTime();
		event.className = message.argument( Argument::ClassName ).toString();
		event.seat = message.argument( Argument::Seat ).toString();
		event.student = message.argument( Argument::Student ).toString();
		event.ip = message.argument( Argument::Ip ).toString();
		event.host = message.argument( Argument::Host ).toString();
		event.type = EventType::call();
		event.detail = message.argument( Argument::Message ).toString();
		store()->append( event );
		if( m_dialog )
		{
			m_dialog->showCall( event.ip, event.seat, event.student );
		}
		Q_EMIT eventReceived();
		return true;
	}

	if( command != Command::ReportEvent ) { return false; }

	ClassroomEvent event;
	event.timestamp = QDateTime::fromString( message.argument( Argument::Timestamp ).toString(), Qt::ISODateWithMs );
	if( event.timestamp.isValid() == false ) { event.timestamp = QDateTime::currentDateTime(); }
	event.sessionId = message.argument( Argument::SessionId ).toString();
	event.className = message.argument( Argument::ClassName ).toString();
	event.seat = message.argument( Argument::Seat ).toString();
	event.student = message.argument( Argument::Student ).toString();
	event.type = message.argument( Argument::EventType ).toString();
	event.detail = message.argument( Argument::Detail ).toString();
	event.app = message.argument( Argument::App ).toString();
	event.host = message.argument( Argument::Host ).toString();
	event.ip = message.argument( Argument::Ip ).toString();

	if( computerControlInterface )
	{
		const auto host = computerControlInterface->computer().hostName();
		event.host = host.isEmpty() ? computerControlInterface->computer().displayName() : host;
		event.ip = host.isEmpty() ? computerControlInterface->computer().hostAddress().toString() : host;
	}

	store()->append( event );
	Q_EMIT eventReceived();
	return true;
}


void ClassroomPlugin::ensureClientUi( VeyonWorkerInterface& worker )
{
	if( m_clientWidget ) { return; }

	m_worker = &worker;
	m_clientWidget = new ClassroomClientWidget;
	m_monitor = new ClassroomMonitor( m_clientWidget );

	connect( m_clientWidget, &ClassroomClientWidget::eventOccurred, this,
			 [this]( const QString& type, const QString& detail, const QString& app ) {
				 reportEvent( type, detail, app );
			 } );
	connect( m_clientWidget, &ClassroomClientWidget::identityChanged, this,
			 [this]( const QString& seat, const QString& student, bool loggedIn ) {
				 m_seat = seat;
				 m_student = student;
				 if( loggedIn == false ) { m_sessionId.clear(); }
			 } );
	connect( m_clientWidget, &ClassroomClientWidget::quitAuthorized, this, [this]() {
		if( m_worker ) { m_worker->sendFeatureMessageReply( Classroom::message( Command::RequestQuit ) ); }
	} );
	connect( m_clientWidget, &ClassroomClientWidget::scoreRequested, this, [this]() {
		if( m_worker == nullptr ) { return; }
		auto query = Classroom::message( Command::QueryScore );
		query.addArgument( Argument::ClassName, m_className )
			 .addArgument( Argument::Seat, m_seat )
			 .addArgument( Argument::Student, m_student )
			 .addArgument( Argument::Host, m_hostName )
			 .addArgument( Argument::Ip, m_ipAddress );
		m_worker->sendFeatureMessageReply( query );
	} );
	connect( m_clientWidget, &ClassroomClientWidget::checkInSubmitted, this, [this]( const QString& name ) {
		if( m_worker == nullptr ) { return; }
		auto reply = Classroom::message( Command::CheckInReply );
		reply.addArgument( Argument::ClassName, m_className )
			.addArgument( Argument::Seat, m_seat )
			.addArgument( Argument::Student, m_student )
			.addArgument( Argument::CheckInName, name )
			.addArgument( Argument::Host, m_hostName )
			.addArgument( Argument::Ip, m_ipAddress );
		m_worker->sendFeatureMessageReply( reply );
		reportEvent( EventType::checkIn(), name, {} );
	} );
	connect( m_clientWidget, &ClassroomClientWidget::callTeacherRequested, this, [this]() {
		if( m_worker == nullptr ) { return; }
		auto call = Classroom::message( Command::CallTeacher );
		call.addArgument( Argument::ClassName, m_className )
			.addArgument( Argument::Seat, m_seat )
			.addArgument( Argument::Student, m_student )
			.addArgument( Argument::Host, m_hostName )
			.addArgument( Argument::Ip, m_ipAddress )
			.addArgument( Argument::Message, tr( "Student is calling the teacher" ) );
		m_worker->sendFeatureMessageReply( call );
	} );
	connect( m_monitor, &ClassroomMonitor::activity, this,
			 [this]( const QString& app, const QString& title, bool browser ) {
				 reportEvent( browser ? EventType::web() : EventType::window(),
							  title.isEmpty() ? app : title, app );
			 } );
	connect( m_monitor, &ClassroomMonitor::blockedActivity, this,
			 [this]( const QString& app, const QString& title, const QString& rule, bool browser ) {
				 reportEvent( browser ? EventType::webBlock() : EventType::appBlock(),
							  QStringLiteral("fuzzy rule '%1' matched: %2").arg( rule, title ), app );
#ifdef Q_OS_WIN
				 // User-mode enforcement only: no driver, no injection. A matched
				 // app/browser is simply terminated and reported to the teacher.
				 if( app.isEmpty() == false )
				 {
					 const auto image = app.contains( QLatin1Char('.') ) ? app : app + QStringLiteral(".exe");
					 QProcess::startDetached( QStringLiteral("taskkill"),
											 QStringList{ QStringLiteral("/F"), QStringLiteral("/IM"), image } );
				 }
#endif
			 } );
}


void ClassroomPlugin::applyWorkerSettings( const QJsonObject& settingsObject )
{
	m_workerSettings = settingsObject;
	if( m_clientWidget ) { m_clientWidget->setSettings( settingsObject ); }
	if( m_monitor ) { m_monitor->setSettings( settingsObject ); }
}


void ClassroomPlugin::reportEvent( const QString& type, const QString& detail, const QString& app )
{
	ClassroomEvent event;
	event.timestamp = QDateTime::currentDateTime();
	event.sessionId = m_sessionId;
	event.className = m_className;
	event.seat = m_seat;
	event.student = m_student;
	event.host = m_hostName;
	event.ip = m_ipAddress;
	event.type = type;
	event.detail = detail;
	event.app = app;

	auto message = Classroom::message( Command::ReportEvent );
	message.addArgument( Argument::Timestamp, event.timestamp.toString( Qt::ISODateWithMs ) )
		   .addArgument( Argument::SessionId, event.sessionId )
		   .addArgument( Argument::ClassName, event.className )
		   .addArgument( Argument::Seat, event.seat )
		   .addArgument( Argument::Student, event.student )
		   .addArgument( Argument::Host, event.host )
		   .addArgument( Argument::Ip, event.ip )
		   .addArgument( Argument::EventType, event.type )
		   .addArgument( Argument::Detail, event.detail )
		   .addArgument( Argument::App, event.app );

	if( m_worker ) { m_worker->sendFeatureMessageReply( message ); }
}


bool ClassroomPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() != clientFeatureUid() ) { return false; }

	m_worker = &worker;
	ensureClientUi( worker );

	const auto settingsFromJson = [&message]() {
		return QJsonDocument::fromJson( message.argument( Argument::SettingsJson ).toString().toUtf8() ).object();
	};

	switch( message.command<Command>() )
	{
	case Command::StartClient:
		applyWorkerSettings( settingsFromJson() );
		m_sessionKey = message.argument( Argument::SessionKey ).toString();
		m_sessionId = message.argument( Argument::SessionId ).toString();
		m_clientWidget->setSessionKey( m_sessionKey );
		m_clientWidget->reactivate();
		VeyonCore::platform().coreFunctions().raiseWindow( m_clientWidget, false );
		reportEvent( EventType::connection(), QStringLiteral("classroom client connected"), {} );
		break;

	case Command::SetSettings:
		applyWorkerSettings( settingsFromJson() );
		break;

	case Command::SetRoster:
		m_clientWidget->setRoster(
			QJsonDocument::fromJson( message.argument( Argument::RosterJson ).toString().toUtf8() ).object() );
		if( m_className.isEmpty() ) { m_className = m_clientWidget->className(); }
		break;

	case Command::SetSessionKey:
		m_sessionKey = message.argument( Argument::SessionKey ).toString();
		m_sessionId = message.argument( Argument::SessionId ).toString();
		m_clientWidget->setSessionKey( m_sessionKey );
		break;

	case Command::SetIdentity:
		m_className = message.argument( Argument::ClassName ).toString();
		m_seat = message.argument( Argument::Seat ).toString();
		m_student = message.argument( Argument::Student ).toString();
		m_sessionId = message.argument( Argument::SessionId ).toString();
		m_clientWidget->setIdentity( m_className, m_seat, m_student );
		m_clientWidget->setLocked( true );
		reportEvent( EventType::assign(), QStringLiteral("signed in by teacher"), {} );
		break;

	case Command::SwitchStudent:
		m_seat = message.argument( Argument::NewSeat ).toString();
		m_student = message.argument( Argument::NewStudent ).toString();
		m_sessionId = message.argument( Argument::SessionId ).toString();
		m_clientWidget->setIdentity( m_className, m_seat, m_student );
		reportEvent( EventType::switchUser(), QStringLiteral("student switched by teacher"), {} );
		break;

	case Command::ClearIdentity:
	case Command::ForceLogout:
		m_clientWidget->clearIdentity();
		m_clientWidget->setLocked( false );
		reportEvent( EventType::logout(), message.argument( Argument::Reason ).toString(), {} );
		m_seat.clear();
		m_student.clear();
		break;

	case Command::LockClient:
	case Command::UnlockClient:
		m_clientWidget->setLocked( message.command<Command>() == Command::LockClient );
		reportEvent( message.command<Command>() == Command::LockClient ? EventType::lock() : EventType::unlock(),
					 QStringLiteral("client lock changed by teacher"), {} );
		break;

	case Command::ToggleAi:
	{
		auto object = m_workerSettings;
		object.insert( SettingKey::aiEnabled(), message.argument( Argument::Enabled ).toBool() );
		applyWorkerSettings( object );
		break;
	}

	case Command::StopClient:
		m_clientWidget->allowHide();
		if( m_monitor ) { m_monitor->stop(); }
		break;

	case Command::ScoreReply:
		m_clientWidget->setScoreHtml( message.argument( Argument::RichText ).toString() );
		break;

	case Command::StartCheckIn:
		m_clientWidget->showCheckIn();
		break;

	case Command::SetUninstallProtection:
	{
		// The worker runs as LocalSystem from VeyonService, so HKLM is writable.
		// The standalone uninstall.exe reads the same value and aborts when set.
		const bool protect = message.argument( Argument::Enabled ).toBool();
#ifdef Q_OS_WIN
		QSettings settings( QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\ClassKeep\\Student"),
							QSettings::Registry64Format );
		settings.setValue( QStringLiteral("UninstallProtection"), protect ? 1 : 0 );
		settings.sync();
#endif
		reportEvent( EventType::protection(),
					 protect ? QStringLiteral("uninstall protection enabled by teacher")
							 : QStringLiteral("uninstall protection cleared by teacher"), {} );
		break;
	}

	case Command::ShowSilencePage:
		m_clientWidget->setSilence( true, message.argument( Argument::Message ).toString() );
		break;

	case Command::HideSilencePage:
		m_clientWidget->setSilence( false, {} );
		break;

	case Command::SetMute:
		ClassroomSystem::setMute( message.argument( Argument::Enabled ).toBool() );
		reportEvent( EventType::mute(), QStringLiteral("audio mute changed by teacher"), {} );
		break;

	case Command::SetInternetBlock:
		ClassroomSystem::setInternetBlock( message.argument( Argument::Enabled ).toBool(),
										  message.argument( Argument::Ip ).toString() );
		reportEvent( EventType::internet(), QStringLiteral("InternetGuard changed by teacher"), {} );
		break;

	case Command::UwfQuery:
	{
		const auto state = ClassroomSystem::uwfState();
		reportEvent( EventType::uwf(), state.raw, {} );
		break;
	}

	case Command::UwfSetFilter:
		ClassroomSystem::setUwfFilter( message.argument( Argument::Enabled ).toBool() );
		reportEvent( EventType::uwf(), QStringLiteral("UWF filter change queued (reboot required)"), {} );
		break;

	case Command::UwfRestart:
		ClassroomSystem::restartSystem();
		break;

	// ---------------------------------------------------------- 2.0.0 quick tools
	case Command::SetExitProtection:
	{
		// "Protection" is CLIENT protection (the student cannot quit the worker).
		// It is deliberately independent from UWF / uninstall protection.
		const auto enabled = message.argument( Argument::Enabled ).toBool();
		auto object = m_workerSettings;
		object.insert( SettingKey::exitProtection(), enabled );
		applyWorkerSettings( object );
		reportEvent( EventType::protection(),
					 enabled ? QStringLiteral("client protection enabled by teacher")
							 : QStringLiteral("client protection disabled by teacher"), {} );
		auto reply = Classroom::message( Command::ProtectionStateReport );
		reply.addArgument( Argument::Enabled, enabled );
		if( m_worker ) { m_worker->sendFeatureMessageReply( reply ); }
		break;
	}

	case Command::OpenSoftware:
	{
		const auto path = message.argument( Argument::SoftwarePath ).toString().trimmed();
		if( path.isEmpty() ) { break; }
		const auto args = message.argument( Argument::SoftwareArgs ).toString();
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
		const auto arguments = QProcess::splitCommand( args );
#else
		const auto arguments = args.split( QLatin1Char(' '), QString::SkipEmptyParts );
#endif
		QProcess::startDetached( path, arguments );
		reportEvent( EventType::software(), path, {} );
		break;
	}

	case Command::OpenWebsite:
	{
		const auto url = message.argument( Argument::Message ).toString().trimmed();
		if( url.isEmpty() ) { break; }
		QDesktopServices::openUrl( QUrl( url ) );
		reportEvent( EventType::website(), url, {} );
		break;
	}

	case Command::SetAppBlockRules:
	case Command::SetWebBlockRules:
	{
		const auto isApp = message.command<Command>() == Command::SetAppBlockRules;
		auto object = m_workerSettings;
		object.insert( isApp ? SettingKey::blockedApps() : SettingKey::blockedWebsites(),
					   QJsonDocument::fromJson(
						   message.argument( Argument::RulesJson ).toString().toUtf8() ).array() );
		applyWorkerSettings( object );
		reportEvent( isApp ? EventType::appBlock() : EventType::webBlock(),
					 QStringLiteral("fuzzy block rules updated by teacher"), {} );
		break;
	}

	default:
		return false;
	}

	return true;
}
