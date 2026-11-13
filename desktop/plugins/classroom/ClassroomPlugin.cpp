/*
 * ClassroomPlugin.cpp - main plugin implementing teacher console, student client
 *                      and behaviour monitoring
 *
 * This file is part of the Ikeback classroom modification.
 */

#include <QtGlobal>

#include <QAbstractSocket>
#include <QAction>
#include <QCursor>
#include <QDockWidget>
#include <QMainWindow>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QMenu>
#include <QProcess>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "ClassroomAiClient.h"
#include "ClassroomClient.h"
#include "ClassroomConfigurationPage.h"
#include "ClassroomData.h"
#include "ClassroomMasterDialog.h"
#include "ClassroomPlugin.h"
#include "ClassroomProtocol.h"
#include "ClassroomScore.h"
#include "ClassroomSignInDialog.h"
#include "ClassroomSystem.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "Filesystem.h"
#include "PlatformCoreFunctions.h"
#include "PlatformPluginInterface.h"
#include "IkebackCore.h"
#include "IkebackMasterInterface.h"
#include "IkebackServerInterface.h"
#include "IkebackWorkerInterface.h"

using namespace Classroom;

//! Give up automatic worker restarts after this many consecutive failures.
static constexpr int MaxWorkerRestartFailures = 5;

IMPLEMENT_CONFIG_PROXY(ClassroomConfiguration)


ClassroomPlugin::ClassroomPlugin( QObject* parent ) :
	QObject( parent ),
	m_configuration( &IkebackCore::config() ),
	m_consoleFeature( Feature( QStringLiteral("ClassroomConsole"),
							   Feature::Flag::Action | Feature::Flag::Master,
							   consoleFeatureUid(), Feature::Uid(),
							   Classroom::productName(), {},
							   tr( "iKeback - 班级、按 IP 登录、日志、积分与控制" ),
							   QStringLiteral(":/core/icon22.png") ) ),
	m_clientFeature( Feature( QStringLiteral("ClassroomClient"),
							  Feature::Flag::Session | Feature::Flag::Service | Feature::Flag::Worker,
							  clientFeatureUid(), Feature::Uid(),
							  tr( "Classroom client" ), {}, {} ) ),
	// Name + seat sign-in replaces Ikeback's username/password logon buttons.
	// Action|Master only (like the console feature): no Worker flag, so no
	// extra worker process; startFeature() never calls prepareLogon().
	m_signInFeature( Feature( QStringLiteral("ClassroomSignIn"),
							  Feature::Flag::Action | Feature::Flag::Master,
							  signInFeatureUid(), Feature::Uid(),
							  tr( "Sign in" ), {},
							  tr( "Sign in a student by name and seat number" ),
							  QStringLiteral(":/core/user-group-new.png") ) ),
	m_signOutFeature( Feature( QStringLiteral("ClassroomSignOut"),
							   Feature::Flag::Action | Feature::Flag::Master,
							   signOutFeatureUid(), Feature::Uid(),
							   tr( "Sign out" ), {},
							   tr( "Sign out the current student" ),
							   QStringLiteral(":/core/edit-delete.png") ) ),
	// 5.2.0: the quick tools and check-in actions are first-class tool-bar
	// buttons. They only open UI in the Master process - no Worker flag, so no
	// extra ikeback Worker process is created.
	m_quickToolsFeature( Feature( QStringLiteral("ClassroomQuickTools"),
								  Feature::Flag::Action | Feature::Flag::Master,
								  quickToolsFeatureUid(), Feature::Uid(),
								  tr( "常用工具" ), {},
								  tr( "广播、UWF、网关、肃静、禁用与评分等参数设置" ),
								  QStringLiteral(":/core/icon22.png") ) ),
								  m_checkInFeature( Feature( QStringLiteral("ClassroomCheckIn"),
								   Feature::Flag::Action | Feature::Flag::Master,
								   checkInFeatureUid(), Feature::Uid(),
								   tr( "签到" ), {},
								   tr( "在所有学生机上开始签到" ),
								   QStringLiteral(":/core/user-group-new.png") ) ),
	m_features( { m_consoleFeature, m_clientFeature, m_signInFeature, m_signOutFeature,
				  m_quickToolsFeature, m_checkInFeature } ),
	m_hostName( QHostInfo::localHostName() )
{
	m_ipAddress = localIpAddress();
	buildQuickActionFeatures();
}


Feature::Uid ClassroomPlugin::quickActionUid( int index )
{
	// Kept in its own UUID range so quick-action buttons can never collide with
	// the protocol features declared in ClassroomProtocol.h.
	return Feature::Uid{ QStringLiteral("d3f1c0de-7a41-4b8e-9c14-%1" )
							 .arg( index, 12, 10, QLatin1Char('0') ) };
}


void ClassroomPlugin::buildQuickActionFeatures()
{
	// Every entry becomes a first-class button on the Ikeback Master tool bar,
	// right next to "Screen demo" and "Remote control". The tool bar is allowed
	// to wrap onto a second row, so the pairs stay side by side.
	struct QuickButton
	{
		const char* id;
		const char* name;
		const char* tooltip;
		const char* icon;
	};

	// One button per function: paired "on"/"off" entries were replaced by a
	// single toggle (see ClassroomMasterDialog::*toggle actions). Broadcast is
	// NOT repeated here - the upstream "屏幕演示" / "远程控制" buttons already
	// do that.
	static const QuickButton buttons[] = {
		{ "protection.toggle", "保护", "点一下切换：学生能否退出 iKeback 卡片",
		  ":/classroom/icons/shield.png" },
		{ "uwf.toggle", "UWF", "点一下切换 Windows 写保护（装软件前先关闭）",
		  ":/classroom/icons/lock.png" },
		{ "gateway.toggle", "外网", "点一下切换学生机外网访问",
		  ":/classroom/icons/globe.png" },
		{ "silence.toggle", "肃静页", "点一下显示或收起肃静页面",
		  ":/classroom/icons/silence.png" },
		{ "appblock.toggle", "禁用应用", "有规则时应用，再点一下解除",
		  ":/classroom/icons/block-app.png" },
		{ "webblock.toggle", "禁用网址", "有规则时应用，再点一下解除",
		  ":/classroom/icons/block-web.png" },
		{ "mute.toggle", "静音", "点一下切换学生机静音",
		  ":/classroom/icons/mute.png" },
		{ "score.p1", "全班 +1", "给全班每人加 1 分",
		  ":/classroom/icons/plus.png" },
		{ "score.m1", "全班 -1", "给全班每人减 1 分",
		  ":/classroom/icons/minus.png" },
		{ "remote.install", "远程装软件", "选择安装包后下发到学生机安装",
		  ":/classroom/icons/install.png" },
	};

	int index = 0;
	for( const auto& button : buttons )
	{
		const auto uid = quickActionUid( index );
		m_quickActionIds.append( QString::fromLatin1( button.id ) );
		// displayName is what the tool bar prints below the icon, so every quick
		// action needs its own label here (an empty displayName renders an
		// unlabelled button).
		const auto label = QCoreApplication::translate( "ClassroomPlugin", button.name );
		m_features.append( Feature( label,
									Feature::Flag::Action | Feature::Flag::Master,
									uid, Feature::Uid(),
									label, {},
									QCoreApplication::translate( "ClassroomPlugin", button.tooltip ),
									QString::fromLatin1( button.icon ) ) );
		++index;
	}
}


ClassroomPlugin::~ClassroomPlugin()
{
	if( m_trayIcon )
	{
		// setContextMenu() does not transfer ownership.
		delete m_trayIcon->contextMenu();
	}
	delete m_trayIcon;
	m_trayIcon = nullptr;
	delete m_clientWidget;
	m_clientWidget = nullptr;
}


void ClassroomPlugin::ensureTrayIcon()
{
	// The ikeback worker always runs in the interactive user session, so it
	// can own the tray icon itself. Ikeback's built-in SystemTrayIcon feature is
	// a separate worker process which is exactly what we want to avoid here.
	if( m_trayIcon ) { return; }
	if( QSystemTrayIcon::isSystemTrayAvailable() == false )
	{
		// Explorer's tray may not be ready when the worker starts. Retry a few
		// times instead of silently giving up on the icon forever.
		if( m_trayIconRetries < 5 )
		{
			++m_trayIconRetries;
			QTimer::singleShot( 3000, this, [this]() { ensureTrayIcon(); } );
		}
		return;
	}

	m_trayIcon = new QSystemTrayIcon( QIcon( QStringLiteral(":/core/icon32.png") ), this );
	m_trayIcon->setToolTip( Classroom::productName() );

	auto* menu = new QMenu;
	menu->addAction( tr( "Show ikeback card" ), this, [this]() {
		if( m_clientWidget ) { m_clientWidget->reactivate(); }
	} );
	menu->addAction( tr( "My score" ), this, [this]() {
		if( m_clientWidget ) { m_clientWidget->requestScore(); }
	} );
	menu->addAction( tr( "AI assistant" ), this, [this]() {
		if( m_clientWidget ) { m_clientWidget->openAiAssistant(); }
	} );
	menu->addSeparator();
	menu->addAction( tr( "Exit (protected)" ), this, [this]() {
		if( m_clientWidget ) { m_clientWidget->requestProtectedQuit(); }
	} );
	m_trayIcon->setContextMenu( menu );

	connect( m_trayIcon, &QSystemTrayIcon::activated, this,
			 [this]( QSystemTrayIcon::ActivationReason reason ) {
				 if( reason != QSystemTrayIcon::Context && m_clientWidget )
				 {
					 m_clientWidget->reactivate();
				 }
			 } );
	m_trayIcon->show();
	updateTrayTooltip();
}


void ClassroomPlugin::updateTrayTooltip()
{
	if( m_trayIcon == nullptr ) { return; }
	QString text = Classroom::productName();
	if( m_student.isEmpty() == false )
	{
		text += QStringLiteral("\n%1 · %2 %3").arg( m_className, tr( "Seat" ), m_seat );
		text += QStringLiteral("\n%1").arg( m_student );
	}
	m_trayIcon->setToolTip( text );
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
	return IkebackCore::filesystem().expandPath( m_configuration.dataDirectory() );
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
							 QString::fromUtf8( QJsonDocument( ClassroomSettings::forStudents(
								 ClassroomSettings::fromConfiguration( m_configuration ) ) ).
												toJson( QJsonDocument::Compact ) ) );
		// The lesson key is generated into settings.json (DPAPI protected), not
		// into the registry-backed Ikeback configuration. Check both so a student
		// card still knows a lesson is active after a fresh console restart.
		message.addArgument( Argument::KeyConfigured,
							 m_configuration.sessionKey().isEmpty() == false ||
							 settings()->object().value( QStringLiteral("sessionKey") ).toString().isEmpty() == false );
	}
	sendFeatureMessage( message, computerControlInterfaces );
	return true;
}


bool ClassroomPlugin::startFeature( IkebackMasterInterface& master, const Feature& feature,
									const ComputerControlInterfaceList& computerControlInterfaces )
{
	// ---------------------------------------------------------------- sign-in
	if( feature.uid() == signInFeatureUid() )
	{
		const auto classNames = roster()->classNames();
		const auto currentClass = settings()->object().value( QStringLiteral("className") ).toString();
		ClassroomSignInDialog dialog( classNames, currentClass, master.mainWindow() );
		if( dialog.exec() != QDialog::Accepted ) { return true; }

		auto message = Classroom::message( Command::SetIdentity );
		message.addArgument( Argument::ClassName, dialog.className() );
		message.addArgument( Argument::Seat, dialog.seat() );
		message.addArgument( Argument::Student, dialog.studentName() );
		message.addArgument( Argument::SessionId,
							 QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") ) );
		sendFeatureMessage( message, computerControlInterfaces );

		ClassroomEvent event;
		event.timestamp = QDateTime::currentDateTime();
		event.className = dialog.className();
		event.seat = dialog.seat();
		event.student = dialog.studentName();
		event.type = EventType::assign();
		event.detail = QStringLiteral("signed in by name and seat");
		store()->append( event );
		return true;
	}

	// --------------------------------------------------------------- sign-out
	if( feature.uid() == signOutFeatureUid() )
	{
		auto message = Classroom::message( Command::ForceLogout );
		message.addArgument( Argument::Reason, tr( "signed out by teacher" ) );
		sendFeatureMessage( message, computerControlInterfaces );

		ClassroomEvent event;
		event.timestamp = QDateTime::currentDateTime();
		event.type = EventType::logout();
		event.detail = tr( "signed out by teacher" );
		store()->append( event );
		return true;
	}

	// ------------------------------------------- first-class quick action buttons
	for( int index = 0; index < m_quickActionIds.size(); ++index )
	{
		if( feature.uid() != quickActionUid( index ) ) { continue; }
		// The parameter widgets (silence text, blocked apps, ...) live in the
		// console, so make sure it exists before running the action.
		if( m_dialog.isNull() ) { startFeature( master, m_consoleFeature, computerControlInterfaces ); }
		if( m_dialog.isNull() ) { return true; }
		m_dialog->runQuickTool( m_quickActionIds.at( index ) );
		return true;
	}

	// ------------------------------------------- tool bar quick tools / check-in
	if( feature.uid() == quickToolsFeatureUid() || feature.uid() == checkInFeatureUid() )
	{
		// The parameter widgets (silence text, blocked apps, software path, ...)
		// belong to the console, so make sure it has been created at least once.
		if( m_dialog.isNull() )
		{
			startFeature( master, m_consoleFeature, computerControlInterfaces );
		}
		if( m_dialog.isNull() ) { return true; }

		if( feature.uid() == checkInFeatureUid() )
		{
			m_dialog->startCheckInFromToolBar();
			return true;
		}

		QMenu menu( master.mainWindow() );
		menu.setToolTipsVisible( true );
		for( const auto& group : m_dialog->quickToolGroups() )
		{
			auto* subMenu = menu.addMenu( group.icon.isEmpty() ? QIcon() : QIcon( group.icon ), group.label );
			for( const auto& entry : group.actions )
			{
				auto* action = subMenu->addAction( entry.label );
				connect( action, &QAction::triggered, &menu, [entry]() {
					if( entry.run ) { entry.run(); }
				} );
			}
		}
		menu.exec();
		return true;
	}

	if( feature.uid() != consoleFeatureUid() ) { return false; }

	// The console always opens as its own window (a pop-up) instead of being
	// docked into the Master window: the monitoring view must stay usable while
	// the console is open, and teachers expect a dialog they can move/close.
	if( m_dialog.isNull() )
	{
		m_dialog = new ClassroomMasterDialog( master, m_configuration, *roster(), *store(), *settings() );
		m_dialog->setWindowFlag( Qt::Window );
		m_dialog->setWindowTitle( Classroom::productName() );
		connect( m_dialog.data(), &ClassroomMasterDialog::hideRequested, m_dialog.data(), [this]() {
			if( m_dialog ) { m_dialog->hide(); }
		} );
	}
	m_dialog->show();
	m_dialog->raise();
	m_dialog->activateWindow();

	// One message, one session-worker wake-up. The old three-message burst
	// re-armed the watchdog and repeatedly called startUnmanagedSessionWorker()
	// while the previous worker was still being torn down (5s kill timer),
	// which is what produced a dozen ikeback-worker.exe processes.
	auto stateMessage = Classroom::message( Command::ApplySession );
	stateMessage.addArgument( Argument::SettingsJson,
							  QString::fromUtf8( QJsonDocument( ClassroomSettings::forStudents( settings()->object() ) ).
												 toJson( QJsonDocument::Compact ) ) );
	stateMessage.addArgument( Argument::RosterJson, roster()->toJson() );
	stateMessage.addArgument( Argument::KeyConfigured,
							  settings()->object().value( QStringLiteral("sessionKey") ).toString().isEmpty() == false );
	stateMessage.addArgument( Argument::SessionId,
							  QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") ) );
	sendFeatureMessage( stateMessage, computerControlInterfaces );

	return true;
}


void ClassroomPlugin::startClient( IkebackServerInterface& server )
{
	auto message = Classroom::message( Command::StartClient );
	message.addArgument( Argument::SettingsJson,
						 QString::fromUtf8( QJsonDocument( ClassroomSettings::forStudents(
							 ClassroomSettings::fromConfiguration( m_configuration ) ) ).
											toJson( QJsonDocument::Compact ) ) );
	message.addArgument( Argument::KeyConfigured, m_configuration.sessionKey().isEmpty() == false );
	message.addArgument( Argument::SessionId,
						 QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") ) );
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
}


void ClassroomPlugin::startSessionFeature( IkebackServerInterface& server )
{
	if( IkebackCore::component() != IkebackCore::Component::Server || m_configuration.enabled() == false )
	{
		return;
	}

	m_server = &server;
	startClient( server );

	m_watchdog = new QTimer( this );
	// 30s matches FeatureWorkerManager's authentication timeout. Polling every
	// 5s used to re-enter startClient() while the previous worker was still
	// authenticating, multiplying ikeback-worker.exe processes.
	m_watchdog->setInterval( 30000 );
	connect( m_watchdog, &QTimer::timeout, this, [this]() {
		if( m_stopAllowed || m_server == nullptr || m_configuration.enabled() == false ) { return; }
		if( m_server->featureWorkerManager().isWorkerRunning( clientFeatureUid() ) )
		{
			m_restartPending = false;
			m_restartFailures = 0;
			return;
		}
		if( m_restartPending ) { return; }

		if( m_restartFailures >= MaxWorkerRestartFailures )
		{
			// Stop the loop instead of spraying processes: a worker that never
			// authenticates is a broken install, not a transient failure, and
			// the next teacher message re-arms this watchdog.
			if( m_watchdog ) { m_watchdog->stop(); }
			vWarning() << "ikeback: student worker failed to start" << m_restartFailures
					   << "times - stopping automatic restarts until the next teacher action";
			return;
		}

		++m_restartFailures;
		startClient( *m_server );
		m_restartPending = true;
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


void ClassroomPlugin::sendAsyncFeatureMessages( IkebackServerInterface& server, const MessageContext& messageContext )
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


bool ClassroomPlugin::handleFeatureMessage( IkebackServerInterface& server, const MessageContext& messageContext,
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
		// A teacher action re-arms the watchdog even after it gave up.
		m_restartFailures = 0;
		m_restartPending = false;
		if( m_watchdog && m_watchdog->isActive() == false ) { m_watchdog->start(); }
	}

	// StopClient may only be delivered to an EXISTING worker. Forwarding it
	// unconditionally used to start a brand new session worker just so that
	// worker could hide the card again.
	if( command != Command::StopClient ||
		server.featureWorkerManager().isWorkerRunning( clientFeatureUid() ) )
	{
		server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
	}
	return true;
}


bool ClassroomPlugin::handleFeatureMessageFromWorker( IkebackServerInterface& server, const FeatureMessage& message )
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
	case Command::AiPrompt:
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

	if( command == Command::AiPrompt )
	{
		// The teacher machine owns the model endpoint and API key. Students only
		// send a prompt + short history over the existing Ikeback feature channel.
		const auto payload = QJsonDocument::fromJson(
			message.argument( Argument::AiPayload ).toString().toUtf8() ).object();
		QVariantList history;
		const auto items = payload.value( QStringLiteral("history") ).toArray();
		for( int index = 0; index < qMin( 20, static_cast<int>( items.size() ) ); ++index )
		{
			const auto item = items.at( index ).toObject();
			QVariantMap entry;
			entry.insert( QStringLiteral("role"), item.value( QStringLiteral("role") ).toString() );
			entry.insert( QStringLiteral("content"), item.value( QStringLiteral("content") ).toString().left( 4000 ) );
			history.append( entry );
		}

		auto* client = new ClassroomAiClient( this );
		ClassroomAiClient::Settings fallback;
		client->setSettings( ClassroomAiClient::Settings::fromJson( settings()->object(), fallback ) );
		connect( client, &ClassroomAiClient::finished, this,
				 [this, client, computerControlInterface]( bool success, const QString& answer ) {
					 client->deleteLater();
					 auto reply = Classroom::message( Command::AiReply );
					 reply.addArgument( Argument::AiSuccess, success )
						  .addArgument( Argument::AiAnswer, answer.left( 8000 ) );
					 if( computerControlInterface ) { computerControlInterface->sendFeatureMessage( reply ); }
				 } );
		client->chat( history, payload.value( QStringLiteral("prompt") ).toString().left( 4000 ) );

		ClassroomEvent event;
		event.timestamp = QDateTime::currentDateTime();
		event.type = Classroom::EventType::ai();
		event.detail = QStringLiteral("AI prompt proxied by the teacher machine");
		event.ip = message.argument( Argument::Ip ).toString();
		event.host = message.argument( Argument::Host ).toString();
		store()->append( event );
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


void ClassroomPlugin::ensureClientUi( IkebackWorkerInterface& worker )
{
	if( m_clientWidget ) { return; }

	m_worker = &worker;
	m_clientWidget = new ClassroomClientWidget;
	m_monitor = new ClassroomMonitor( m_clientWidget );
	ensureTrayIcon();

	connect( m_clientWidget, &ClassroomClientWidget::eventOccurred, this,
			 [this]( const QString& type, const QString& detail, const QString& app ) {
				 reportEvent( type, detail, app );
			 } );
	connect( m_clientWidget, &ClassroomClientWidget::identityChanged, this,
			 [this]( const QString& seat, const QString& student, bool loggedIn ) {
				 m_seat = seat;
				 m_student = student;
				 if( loggedIn == false ) { m_sessionId.clear(); }
				 updateTrayTooltip();
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
	connect( m_clientWidget, &ClassroomClientWidget::aiPromptRequested, this,
			 [this]( const QString& prompt, const QVariantList& history ) {
				 if( m_worker == nullptr ) { return; }
				 QJsonArray items;
				 for( const auto& value : history )
				 {
					 const auto entry = value.toMap();
					 items.append( QJsonObject{
						 { QStringLiteral("role"), entry.value( QStringLiteral("role") ).toString() },
						 { QStringLiteral("content"), entry.value( QStringLiteral("content") ).toString().left( 4000 ) } } );
				 }
				 auto request = Classroom::message( Command::AiPrompt );
				 request.addArgument( Argument::AiPayload,
									  QString::fromUtf8( QJsonDocument( QJsonObject{
										  { QStringLiteral("prompt"), prompt },
										  { QStringLiteral("history"), items } } ).toJson( QJsonDocument::Compact ) ) )
						.addArgument( Argument::Ip, m_ipAddress )
						.addArgument( Argument::Host, m_hostName );
				 m_worker->sendFeatureMessageReply( request );
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
				 // Short rules never reach this point (ClassroomMonitor
				 // drops them), but keep the guard here as well.
				 if( app.size() >= 4 && rule.size() >= 4 )
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


bool ClassroomPlugin::handleFeatureMessage( IkebackWorkerInterface& worker, const FeatureMessage& message )
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
		m_keyConfigured = message.argument( Argument::KeyConfigured ).toBool();
		m_sessionId = message.argument( Argument::SessionId ).toString();
		m_clientWidget->setKeyConfigured( m_keyConfigured );
		m_clientWidget->reactivate();
		IkebackCore::platform().coreFunctions().raiseWindow( m_clientWidget, false );
		reportEvent( EventType::connection(), QStringLiteral("classroom client connected"), {} );
		break;

	case Command::ApplySession:
		// Combined console-open state. Keeping it to one message avoids
		// repeatedly restarting the unmanaged session worker.
		applyWorkerSettings( settingsFromJson() );
		m_clientWidget->setRoster(
			QJsonDocument::fromJson( message.argument( Argument::RosterJson ).toString().toUtf8() ).object() );
		if( m_className.isEmpty() ) { m_className = m_clientWidget->className(); }
		m_keyConfigured = message.argument( Argument::KeyConfigured ).toBool();
		m_sessionId = message.argument( Argument::SessionId ).toString();
		m_clientWidget->setKeyConfigured( m_keyConfigured );
		m_clientWidget->reactivate();
		updateTrayTooltip();
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
		// Legacy command name, new payload: a heartbeat carrying only a boolean.
		m_keyConfigured = message.argument( Argument::KeyConfigured ).toBool();
		m_sessionId = message.argument( Argument::SessionId ).toString();
		m_clientWidget->setKeyConfigured( m_keyConfigured );
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

	case Command::AiReply:
		m_clientWidget->deliverAiReply( message.argument( Argument::AiSuccess ).toBool(),
										message.argument( Argument::AiAnswer ).toString() );
		break;

	case Command::StartCheckIn:
		m_clientWidget->showCheckIn();
		break;

	case Command::SetUninstallProtection:
	{
		// The floating card runs in the interactive user's session. HKLM writes
		// therefore only succeed when that account has administrative rights;
		// the result is read back and reported instead of silently succeeding.
		const bool protect = message.argument( Argument::Enabled ).toBool();
		bool confirmed = false;
#ifdef Q_OS_WIN
		QSettings settings( QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\ikeback\\Student"),
							QSettings::Registry64Format );
		settings.setValue( QStringLiteral("UninstallProtection"), protect ? 1 : 0 );
		settings.sync();
		const auto status = settings.status();
		const auto readBack = settings.value( QStringLiteral("UninstallProtection"), -1 ).toInt();
		confirmed = status == QSettings::NoError && readBack == ( protect ? 1 : 0 );
		if( confirmed == false )
		{
			qWarning( "ikeback: SetUninstallProtection failed (status=%d, readback=%d, lastError=%s)",
					  static_cast<int>( status ), readBack, qPrintable( ClassroomSystem::lastError() ) );
		}
#endif
		reportEvent( EventType::protection(),
					 confirmed ? ( protect ? QStringLiteral("uninstall protection enabled by teacher")
										   : QStringLiteral("uninstall protection cleared by teacher") )
							   : QStringLiteral("uninstall protection FAILED: HKLM write not confirmed"), {} );
		auto reply = Classroom::message( Command::ProtectionStateReport );
		reply.addArgument( Argument::Enabled, protect )
			 .addArgument( Argument::Result, confirmed ? QStringLiteral("ok") : QStringLiteral("failed") );
		if( m_worker ) { m_worker->sendFeatureMessageReply( reply ); }
		break;
	}

	case Command::ShowSilencePage:
		m_clientWidget->setSilence( true, message.argument( Argument::Message ).toString() );
		break;

	case Command::HideSilencePage:
		m_clientWidget->setSilence( false, {} );
		break;

	case Command::SetMute:
		if( ClassroomSystem::setMute( message.argument( Argument::Enabled ).toBool() ) == false )
		{
			qWarning( "ikeback: SetMute failed: %s", qPrintable( ClassroomSystem::lastError() ) );
		}
		reportEvent( EventType::mute(), QStringLiteral("audio mute changed by teacher"), {} );
		break;

	case Command::SetInternetBlock:
	{
		// A second line of defence: even a compromised teacher console or a
		// forged feature message cannot turn remoteip= into a CIDR range.
		const auto teacherIp = Classroom::sanitizeUnicastIp( message.argument( Argument::Ip ).toString() );
		if( ClassroomSystem::setInternetBlock( message.argument( Argument::Enabled ).toBool(), teacherIp ) == false )
		{
			qWarning( "ikeback: SetInternetBlock failed: %s", qPrintable( ClassroomSystem::lastError() ) );
		}
		reportEvent( EventType::internet(), QStringLiteral("InternetGuard changed by teacher"), {} );
		break;
	}

	case Command::UwfQuery:
	{
		const auto state = ClassroomSystem::uwfState();
		reportEvent( EventType::uwf(), state.raw, {} );
		break;
	}

	case Command::UwfSetFilter:
		if( ClassroomSystem::setUwfFilter( message.argument( Argument::Enabled ).toBool() ) == false )
		{
			qWarning( "ikeback: UwfSetFilter failed: %s", qPrintable( ClassroomSystem::lastError() ) );
		}
		reportEvent( EventType::uwf(), QStringLiteral("UWF filter change queued (reboot required)"), {} );
		break;

	case Command::UwfRestart:
		ClassroomSystem::restartSystem();
		break;

	// ---------------------------------------------------------- 5.2.0 quick tools
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
		const QFileInfo info( path );
		// Only an existing .exe may be launched; scripts and arbitrary paths
		// would turn this command into remote command execution.
		if( path.isEmpty() || info.isFile() == false ||
			info.suffix().compare( QStringLiteral("exe"), Qt::CaseInsensitive ) != 0 )
		{
			qWarning( "ikeback: OpenSoftware rejected a non-.exe or missing path" );
			break;
		}
		const auto args = message.argument( Argument::SoftwareArgs ).toString();
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
		const auto arguments = QProcess::splitCommand( args );
#else
		const auto arguments = args.split( QLatin1Char(' '), QString::SkipEmptyParts );
#endif
		QProcess::startDetached( info.absoluteFilePath(), arguments );
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
