/*
 * ClassroomMasterDialog.cpp - teacher console for classes, logs, analysis and AI
 *
 * This file is part of the Veyon classroom modification.
 */

#include <algorithm>
#include <functional>

#include <QAbstractItemView>
#include <QAbstractSocket>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QIcon>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QNetworkInterface>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPair>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>
#include <QVBoxLayout>

#include "ClassroomAiClient.h"
#include "ClassroomBridge.h"
#include "ClassroomConfiguration.h"
#include "ClassroomData.h"
#include "ClassroomMasterDialog.h"
#include "ClassroomProtocol.h"
#include "ClassroomScorePanel.h"
#include "ClassroomSeatGrid.h"
#include "ClassroomSystem.h"
#include "ComputerControlInterface.h"
#include "FeatureManager.h"
#include "FeatureProviderInterface.h"
#include "Filesystem.h"
#include "NetworkObject.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"

using namespace Classroom;


ClassroomMasterDialog::ClassroomMasterDialog( VeyonMasterInterface& master,
											 ClassroomConfiguration& configuration,
											 ClassroomRoster& roster,
											 ClassroomLogStore& store,
											 ClassroomSettings& settings,
											 QWidget* parent ) :
	QDialog( parent ),
	m_master( master ),
	m_configuration( configuration ),
	m_roster( roster ),
	m_store( store ),
	m_settings( settings ),
	m_testAiClient( new ClassroomAiClient( this ) ),
	m_refreshTimer( new QTimer( this ) )
{
	setWindowTitle( Classroom::productName() );
	setWindowIcon( QIcon( QStringLiteral(":/core/icon32.png") ) );
	resize( 1024, 720 );

	m_tabs = new QTabWidget( this );
	setupClassTab();
	setupLogTab();
	setupAnalysisTab();
	setupAiTab();
	setupMonitorTab();
	setupSystemTab();

	m_scorePanel = new ClassroomScorePanel( dataDirectory(), m_roster, m_store, settingsJson(), m_tabs );
	m_tabs->addTab( m_scorePanel, tr( "Score & reports" ) );

	setupSeatTab();

	auto* closeButton = new QPushButton( tr( "Close" ), this );
	connect( closeButton, &QPushButton::clicked, this, &QDialog::close );
	auto* footer = new QHBoxLayout;
	footer->addStretch( 1 );
	footer->addWidget( closeButton );

	auto* layout = new QVBoxLayout( this );
	layout->addWidget( m_tabs, 1 );
	layout->addLayout( footer );

	currentSessionId();
	refreshRosterTable();
	m_scorePanel->setClassName( currentClassName() );
	if( m_bridge ) { m_bridge->setClassName( currentClassName() ); }
	if( m_seatGrid ) { m_seatGrid->setClassName( currentClassName() ); }
	refreshSeatGrid();
	refreshCheckIn();
	refreshMachines();
	refreshLogs();
	refreshAnalysis();
	refreshSettingsWidgets();

	connect( m_refreshTimer, &QTimer::timeout, this, [this]() {
		refreshStatus();
		if( ++m_refreshTicks % 6 == 0 ) { pushKey(); }
	} );
	m_refreshTimer->start( 5000 );

	connect( m_testAiClient, &ClassroomAiClient::finished, this, [this]( bool success, const QString& reply ) {
		m_aiTestResult->setText( success ? tr( "Connection OK: %1" ).arg( reply.left( 160 ) )
										: tr( "Connection failed: %1" ).arg( reply ) );
	} );
}


ClassroomMasterDialog::~ClassroomMasterDialog()
{
	for( auto it = m_directInterfaces.begin(); it != m_directInterfaces.end(); ++it )
	{
		if( it.value() ) { it.value()->stop(); }
	}
	m_directInterfaces.clear();
}


QString ClassroomMasterDialog::dataDirectory() const
{
	const auto configured = m_settings.object().value( SettingKey::dataDirectory() ).
							toString( m_configuration.dataDirectory() );
	return VeyonCore::filesystem().expandPath( configured );
}


QJsonObject ClassroomMasterDialog::settingsJson() const
{
	return m_settings.object();
}


QString ClassroomMasterDialog::sessionKey() const
{
	return m_settings.object().value( QStringLiteral("sessionKey") ).toString();
}


QString ClassroomMasterDialog::currentClassName() const
{
	return m_classCombo ? m_classCombo->currentText() : QString{};
}


QString ClassroomMasterDialog::currentSessionId()
{
	if( m_sessionId.isEmpty() )
	{
		m_sessionId = QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") );
	}
	if( m_sessionLabel )
	{
		m_sessionLabel->setText( tr( "Session: %1" ).arg( m_sessionId ) );
	}
	return m_sessionId;
}


QString ClassroomMasterDialog::ipOfInterface( const ComputerControlInterface* controlInterface )
{
	if( controlInterface == nullptr ) { return {}; }
	const auto host = controlInterface->computer().hostName();
	if( host.isEmpty() == false ) { return host; }
	return controlInterface->computer().hostAddress().toString();
}


QStringList ClassroomMasterDialog::onlineIps() const
{
	QStringList ips;
	for( const auto& controlInterface : m_master.allComputerControlInterfaces() )
	{
		const auto ip = ipOfInterface( controlInterface.data() );
		if( ip.isEmpty() == false ) { ips.append( ip ); }
	}
	return ips;
}


QStringList ClassroomMasterDialog::selectedIps() const
{
	QStringList ips;
	const auto selected = m_rosterTable->selectionModel()->selectedRows();
	if( selected.isEmpty() )
	{
		for( int row = 0; row < m_rosterTable->rowCount(); ++row )
		{
			const auto* item = m_rosterTable->item( row, 2 );
			if( item && item->text().trimmed().isEmpty() == false ) { ips.append( item->text().trimmed() ); }
		}
	}
	else
	{
		for( const auto& index : selected )
		{
			const auto* item = m_rosterTable->item( index.row(), 2 );
			if( item && item->text().trimmed().isEmpty() == false ) { ips.append( item->text().trimmed() ); }
		}
	}
	return ips;
}


void ClassroomMasterDialog::sendToAll( const FeatureMessage& message )
{
	for( const auto& controlInterface : m_master.allComputerControlInterfaces() )
	{
		controlInterface->sendFeatureMessage( message );
	}
}


void ClassroomMasterDialog::sendToIp( const QString& ip, const FeatureMessage& message )
{
	if( ip.trimmed().isEmpty() ) { return; }
	const auto address = ip.trimmed();

	bool found = false;
	for( const auto& controlInterface : m_master.allComputerControlInterfaces() )
	{
		if( ipOfInterface( controlInterface.data() ) == address )
		{
			controlInterface->sendFeatureMessage( message );
			found = true;
		}
	}
	if( found )
	{
		// The computer has appeared in the master's directory; drop the temporary connection.
		auto obsolete = m_directInterfaces.take( address );
		if( obsolete ) { obsolete->stop(); }
		return;
	}

	if( m_directInterfaces.contains( address ) == false )
	{
		Computer customComputer;
		customComputer.setHostAddress( address );
		customComputer.setDisplayName( address );
		auto direct = ComputerControlInterface::Pointer::create( customComputer );
		direct->start( {}, ComputerControlInterface::UpdateMode::FeatureControlOnly );
		m_directInterfaces.insert( address, direct );
	}
	queueDirectMessage( address, message, 20 );
}


void ClassroomMasterDialog::queueDirectMessage( const QString& ip, const FeatureMessage& message, int retries )
{
	const auto direct = m_directInterfaces.value( ip );
	if( direct.isNull() ) { return; }

	if( direct->connection() && direct->connection()->isConnected() )
	{
		direct->sendFeatureMessage( message );
		return;
	}
	if( retries <= 0 ) { return; }

	const auto pending = QSharedPointer<FeatureMessage>::create( message );
	QTimer::singleShot( 500, this, [this, ip, pending, retries]() {
		queueDirectMessage( ip, *pending, retries - 1 );
	} );
}


void ClassroomMasterDialog::appendTeacherEvent( const QString& type, const QString& detail,
												const QString& ip, const QString& seat, const QString& student )
{
	ClassroomEvent event;
	event.timestamp = QDateTime::currentDateTime();
	event.sessionId = currentSessionId();
	event.className = currentClassName();
	event.seat = seat;
	event.student = student;
	event.host = tr( "teacher" );
	event.ip = ip;
	event.type = type;
	event.detail = detail;
	m_store.append( event );

	if( ip.isEmpty() == false )
	{
		if( type == EventType::login() || type == EventType::assign() || type == EventType::switchUser() )
		{
			m_status[ip] = tr( "signed in: %1 (%2)" ).arg( student, seat );
		}
		else if( type == EventType::logout() )
		{
			m_status.remove( ip );
		}
	}
}


void ClassroomMasterDialog::setupClassTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );

	m_classCombo = new QComboBox( page );
	auto* classRow = new QHBoxLayout;
	classRow->addWidget( new QLabel( tr( "Class:" ), page ) );
	classRow->addWidget( m_classCombo, 1 );
	auto* addClassButton = new QPushButton( tr( "New class" ), page );
	auto* importButton = new QPushButton( tr( "Import CSV" ), page );
	auto* exportButton = new QPushButton( tr( "Export CSV" ), page );
	auto* saveRosterButton = new QPushButton( tr( "Save roster" ), page );
	classRow->addWidget( addClassButton );
	classRow->addWidget( importButton );
	classRow->addWidget( exportButton );
	classRow->addWidget( saveRosterButton );
	layout->addLayout( classRow );

	connect( m_classCombo, &QComboBox::currentTextChanged, this, [this]() {
		refreshRosterTable();
		refreshStatus();
		if( m_scorePanel ) { m_scorePanel->setClassName( currentClassName() ); }
		if( m_bridge ) { m_bridge->setClassName( currentClassName() ); }
		if( m_seatGrid ) { m_seatGrid->setClassName( currentClassName() ); }
		m_checkInNames.clear();
		m_checkInTimes.clear();
		m_checkInStarted = {};
		refreshCheckIn();
		refreshSeatGrid();
	} );
	connect( addClassButton, &QPushButton::clicked, this, [this]() { addClass(); } );
	connect( importButton, &QPushButton::clicked, this, [this]() { importRoster(); } );
	connect( exportButton, &QPushButton::clicked, this, [this]() { exportRoster(); } );
	connect( saveRosterButton, &QPushButton::clicked, this, [this]() { saveRoster(); } );

	auto* sessionRow = new QHBoxLayout;
	auto* newSessionButton = new QPushButton( tr( "Start new lesson" ), page );
	auto* generateKeyButton = new QPushButton( tr( "Generate key" ), page );
	auto* pushKeyButton = new QPushButton( tr( "Push key" ), page );
	auto* pushRosterButton = new QPushButton( tr( "Push roster" ), page );
	auto* pushSettingsButton = new QPushButton( tr( "Push settings" ), page );
	auto* syncButton = new QPushButton( tr( "Sync IPs to directory" ), page );
	m_keyLabel = new QLabel( page );
	m_sessionLabel = new QLabel( page );
	sessionRow->addWidget( newSessionButton );
	sessionRow->addWidget( generateKeyButton );
	sessionRow->addWidget( pushKeyButton );
	sessionRow->addWidget( pushRosterButton );
	sessionRow->addWidget( pushSettingsButton );
	sessionRow->addWidget( syncButton );
	sessionRow->addStretch( 1 );
	sessionRow->addWidget( m_keyLabel );
	sessionRow->addWidget( m_sessionLabel );
	layout->addLayout( sessionRow );

	connect( newSessionButton, &QPushButton::clicked, this, [this]() { startNewSession(); } );
	connect( generateKeyButton, &QPushButton::clicked, this, [this]() { generateKey(); } );
	connect( pushKeyButton, &QPushButton::clicked, this, [this]() { pushKey(); } );
	connect( pushRosterButton, &QPushButton::clicked, this, [this]() { pushRoster(); } );
	connect( pushSettingsButton, &QPushButton::clicked, this, [this]() { pushSettings(); } );
	connect( syncButton, &QPushButton::clicked, this, [this]() {
		saveRoster();
		refreshStatus();
	} );

	m_rosterTable = new QTableWidget( 0, 5, page );
	m_rosterTable->setHorizontalHeaderLabels( { tr( "Seat" ), tr( "Name" ), tr( "IP address" ),
												tr( "Status" ), tr( "Last activity" ) } );
	m_rosterTable->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
	m_rosterTable->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_rosterTable->setSelectionMode( QAbstractItemView::ExtendedSelection );
	layout->addWidget( m_rosterTable, 1 );

	auto* actionRow = new QHBoxLayout;
	auto* assignButton = new QPushButton( tr( "Sign in by IP" ), page );
	auto* batchLoginButton = new QPushButton( tr( "Batch sign in" ), page );
	auto* batchLogoutButton = new QPushButton( tr( "Batch sign out" ), page );
	auto* forceLogoutButton = new QPushButton( tr( "Force sign out" ), page );
	auto* switchButton = new QPushButton( tr( "Change student" ), page );
	auto* lockButton = new QPushButton( tr( "Lock all" ), page );
	auto* unlockButton = new QPushButton( tr( "Unlock all" ), page );
	auto* aiOnButton = new QPushButton( tr( "Enable AI (all)" ), page );
	auto* aiOffButton = new QPushButton( tr( "Disable AI (all)" ), page );
	auto* refreshButton = new QPushButton( tr( "Refresh" ), page );
	for( auto* button : { assignButton, batchLoginButton, batchLogoutButton, forceLogoutButton, switchButton,
						  lockButton, unlockButton, aiOnButton, aiOffButton, refreshButton } )
	{
		actionRow->addWidget( button );
	}
	layout->addLayout( actionRow );

	connect( assignButton, &QPushButton::clicked, this, [this]() { assignByIp(); } );
	connect( batchLoginButton, &QPushButton::clicked, this, [this]() { batchLogin(); } );
	connect( batchLogoutButton, &QPushButton::clicked, this, [this]() { batchLogout(); } );
	connect( forceLogoutButton, &QPushButton::clicked, this, [this]() { forceLogout(); } );
	connect( switchButton, &QPushButton::clicked, this, [this]() { switchStudent(); } );
	connect( lockButton, &QPushButton::clicked, this, [this]() { lockAll( true ); } );
	connect( unlockButton, &QPushButton::clicked, this, [this]() { lockAll( false ); } );
	connect( aiOnButton, &QPushButton::clicked, this, [this]() { toggleAi( true ); } );
	connect( aiOffButton, &QPushButton::clicked, this, [this]() { toggleAi( false ); } );
	connect( refreshButton, &QPushButton::clicked, this, [this]() { refreshStatus(); refreshMachines(); } );

	auto* quickRow = new QHBoxLayout;
	m_quickIp = new QLineEdit( page );
	m_quickIp->setPlaceholderText( tr( "IP address" ) );
	m_quickSeat = new QLineEdit( page );
	m_quickSeat->setPlaceholderText( tr( "Seat number" ) );
	m_quickName = new QLineEdit( page );
	m_quickName->setPlaceholderText( tr( "Student name" ) );
	auto* quickButton = new QPushButton( tr( "Quick sign in by IP" ), page );
	quickRow->addWidget( new QLabel( tr( "Quick:" ), page ) );
	quickRow->addWidget( m_quickIp );
	quickRow->addWidget( m_quickSeat );
	quickRow->addWidget( m_quickName );
	quickRow->addWidget( quickButton );
	layout->addLayout( quickRow );
	connect( quickButton, &QPushButton::clicked, this, [this]() { quickLoginByIp(); } );

	m_tabs->addTab( page, tr( "Class & sign-in" ) );
}


void ClassroomMasterDialog::refreshRosterTable()
{
	const QSignalBlocker blocker( m_classCombo );
	const auto previous = m_classCombo->currentText();
	m_classCombo->clear();
	m_classCombo->addItems( m_roster.classNames() );
	if( previous.isEmpty() == false )
	{
		m_classCombo->setCurrentText( previous );
	}

	const auto* item = m_roster.findClass( currentClassName() );
	const QSignalBlocker tableBlocker( m_rosterTable );
	m_rosterTable->setRowCount( item ? item->students.size() : 0 );
	if( item == nullptr ) { return; }

	for( int row = 0; row < item->students.size(); ++row )
	{
		const auto& student = item->students.at( row );
		m_rosterTable->setItem( row, 0, new QTableWidgetItem( student.seat ) );
		m_rosterTable->setItem( row, 1, new QTableWidgetItem( student.name ) );
		m_rosterTable->setItem( row, 2, new QTableWidgetItem( student.ip ) );
		m_rosterTable->setItem( row, 3, new QTableWidgetItem( m_status.value( student.ip, tr( "not signed in" ) ) ) );
		m_rosterTable->setItem( row, 4, new QTableWidgetItem( {} ) );
	}
	refreshStatus();
}


void ClassroomMasterDialog::refreshStatus()
{
	refreshSeatGrid();
	const auto states = m_store.identityStates();
	m_status.clear();
	for( auto it = states.constBegin(); it != states.constEnd(); ++it )
	{
		m_status.insert( it.key(), tr( "signed in: %1" ).arg( it.value() ) );
	}
	const auto lastActivity = m_store.lastActivities();

	const QSignalBlocker blocker( m_rosterTable );
	for( int row = 0; row < m_rosterTable->rowCount(); ++row )
	{
		auto* ipItem = m_rosterTable->item( row, 2 );
		if( ipItem == nullptr ) { continue; }
		const auto ip = ipItem->text().trimmed();
		if( auto* statusItem = m_rosterTable->item( row, 3 ) )
		{
			statusItem->setText( m_status.value( ip, tr( "not signed in" ) ) );
		}
		if( auto* activityItem = m_rosterTable->item( row, 4 ) )
		{
			activityItem->setText( lastActivity.contains( ip ) ?
								   lastActivity.value( ip ).toString( QStringLiteral("HH:mm:ss") ) : QString{} );
		}
	}

	if( m_keyLabel )
	{
		const auto key = sessionKey();
		m_keyLabel->setText( tr( "Key: %1" ).arg( key.isEmpty() ? tr( "none" ) : key ) );
	}
}


void ClassroomMasterDialog::addClass()
{
	bool ok = false;
	const auto name = QInputDialog::getText( this, windowTitle(), tr( "New class name:" ), QLineEdit::Normal, {}, &ok );
	if( ok == false || name.trimmed().isEmpty() ) { return; }
	if( m_roster.findClass( name.trimmed() ) != nullptr ) { return; }
	ClassroomClass newClass;
	newClass.name = name.trimmed();
	m_roster.classes().append( newClass );
	m_roster.save();
	refreshRosterTable();
}


void ClassroomMasterDialog::saveRoster()
{
	auto* item = m_roster.findClass( currentClassName() );
	if( item == nullptr ) { return; }
	QVector<ClassroomStudent> students;
	for( int row = 0; row < m_rosterTable->rowCount(); ++row )
	{
		ClassroomStudent student;
		student.seat = m_rosterTable->item( row, 0 ) ? m_rosterTable->item( row, 0 )->text().trimmed() : QString{};
		student.name = m_rosterTable->item( row, 1 ) ? m_rosterTable->item( row, 1 )->text().trimmed() : QString{};
		student.ip = m_rosterTable->item( row, 2 ) ? m_rosterTable->item( row, 2 )->text().trimmed() : QString{};
		if( student.seat.isEmpty() && student.name.isEmpty() ) { continue; }
		students.append( student );
	}
	item->students = students;
	if( m_roster.save() == false )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Could not save the roster to %1" ).arg( m_roster.filePath() ) );
		return;
	}
	pushRoster();
	registerComputers( item->students );
}


void ClassroomMasterDialog::registerComputers( const QVector<ClassroomStudent>& students )
{
	const auto className = currentClassName();
	if( className.isEmpty() ) { return; }

	auto& configuration = VeyonCore::config();
	if( configuration.isStoreWritable() == false )
	{
		QMessageBox::information( this, windowTitle(),
								  tr( "Veyon Master cannot write the global computer directory. "
									  "Run it as administrator or use Veyon Configurator. "
									  "IP addresses can still be reached directly." ) );
		return;
	}
	auto objects = configuration.value( QStringLiteral("NetworkObjects"), QStringLiteral("BuiltinDirectory"),
										QJsonArray() ).toJsonArray();

	QUuid locationUid;
	for( const auto& value : objects )
	{
		const auto object = value.toObject();
		if( object.value( QStringLiteral("Type") ).toInt() == static_cast<int>( NetworkObject::Type::Location ) &&
			object.value( QStringLiteral("Name") ).toString() == className )
		{
			locationUid = QUuid( object.value( QStringLiteral("Uid") ).toString() );
		}
	}

	if( locationUid.isNull() )
	{
		locationUid = QUuid::createUuid();
		objects.append( NetworkObject( NetworkObject::Type::Location, className, {}, {}, {},
									   locationUid, QUuid() ).toJson() );
	}

	// Replace this class' hosts instead of accumulating stale IPs from previous lessons.
	QJsonArray otherObjects;
	for( const auto& value : objects )
	{
		const auto object = value.toObject();
		const auto parentUid = QUuid( object.value( QStringLiteral("ParentUid") ).toString() );
		if( parentUid == locationUid &&
			object.value( QStringLiteral("Type") ).toInt() == static_cast<int>( NetworkObject::Type::Host ) )
		{
			continue;
		}
		otherObjects.append( value );
	}

	QSet<QString> writtenAddresses;
	QJsonArray newObjects = otherObjects;
	for( const auto& student : students )
	{
		if( student.ip.isEmpty() || writtenAddresses.contains( student.ip ) ) { continue; }
		const auto displayName = QStringLiteral("%1 %2").arg( student.seat, student.name ).trimmed();
		newObjects.append( NetworkObject( NetworkObject::Type::Host, displayName, student.ip, {}, {},
										  QUuid::createUuid(), locationUid ).toJson() );
		writtenAddresses.insert( student.ip );
	}

	if( newObjects == objects ) { return; }
	configuration.setValue( QStringLiteral("NetworkObjects"), newObjects, QStringLiteral("BuiltinDirectory") );
	configuration.flushStore();
	if( auto* directory = VeyonCore::networkObjectDirectoryManager().configuredDirectory() )
	{
		directory->update();
	}
}


void ClassroomMasterDialog::importRoster()
{
	const auto fileName = QFileDialog::getOpenFileName( this, tr( "Import class roster" ), {},
													   tr( "CSV files (*.csv);;All files (*)" ) );
	if( fileName.isEmpty() || currentClassName().isEmpty() ) { return; }
	QString error;
	if( m_roster.importCsv( fileName, currentClassName(), &error ) == false )
	{
		QMessageBox::warning( this, windowTitle(), error );
		return;
	}
	m_roster.save();
	refreshRosterTable();
	pushRoster();
	if( const auto* imported = m_roster.findClass( currentClassName() ) )
	{
		registerComputers( imported->students );
	}
}


void ClassroomMasterDialog::exportRoster()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export class roster" ),
													   QDir( dataDirectory() ).filePath( currentClassName() + QStringLiteral(".csv") ),
													   tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() ) { return; }
	QString error;
	if( m_roster.exportCsv( fileName, currentClassName(), &error ) == false )
	{
		QMessageBox::warning( this, windowTitle(), error );
	}
}


void ClassroomMasterDialog::startNewSession()
{
	m_sessionId.clear();
	currentSessionId();
	// A new lesson always rotates the key: any device holding the previous
	// lesson key immediately loses access to the bridge as well.
	generateKey();
	for( const auto& ip : selectedIps() ) { pushLogout( ip ); }
	pushSettings();
	pushRoster();
	pushKey();
	appendTeacherEvent( EventType::connection(), tr( "new lesson started for class %1" ).arg( currentClassName() ) );
	QMessageBox::information( this, windowTitle(),
							  tr( "A new lesson has been started. Settings, roster and key were pushed to all computers." ) );
}


void ClassroomMasterDialog::generateKey()
{
	auto object = m_settings.object();
	object.insert( QStringLiteral("sessionKey"), QUuid::createUuid().toString( QUuid::WithoutBraces ).left( 16 ).toUpper() );
	m_settings.setObject( object );
	m_settings.save();
	if( m_bridge ) { m_bridge->setSettings( settingsJson() ); }
	refreshStatus();
	pushKey();
	appendTeacherEvent( EventType::lock(), tr( "new connection key generated" ) );
}


void ClassroomMasterDialog::pushSettings()
{
	auto message = Classroom::message( Command::SetSettings );
	message.addArgument( Argument::SettingsJson,
						 QString::fromUtf8( QJsonDocument( settingsJson() ).toJson( QJsonDocument::Compact ) ) );
	sendToAll( message );
}


void ClassroomMasterDialog::pushRoster()
{
	auto message = Classroom::message( Command::SetRoster );
	message.addArgument( Argument::RosterJson, m_roster.toJson() );
	sendToAll( message );
}


void ClassroomMasterDialog::pushKey()
{
	auto message = Classroom::message( Command::SetSessionKey );
	message.addArgument( Argument::SessionKey, sessionKey() );
	message.addArgument( Argument::SessionId, currentSessionId() );
	sendToAll( message );
}


void ClassroomMasterDialog::pushIdentity( const QString& ip, const QString& seat, const QString& name )
{
	auto message = Classroom::message( Command::SetIdentity );
	message.addArgument( Argument::ClassName, currentClassName() );
	message.addArgument( Argument::Seat, seat );
	message.addArgument( Argument::Student, name );
	message.addArgument( Argument::SessionId, currentSessionId() );
	sendToIp( ip, message );
	appendTeacherEvent( EventType::assign(), tr( "teacher signed in %1 at %2" ).arg( name, ip ), ip, seat, name );
}


void ClassroomMasterDialog::pushLogout( const QString& ip )
{
	auto message = Classroom::message( Command::ForceLogout );
	message.addArgument( Argument::Reason, tr( "signed out by the teacher" ) );
	sendToIp( ip, message );
	appendTeacherEvent( EventType::logout(), tr( "teacher signed out the student" ), ip );
}


void ClassroomMasterDialog::assignByIp()
{
	pushSettings();
	pushRoster();
	pushKey();

	const auto* item = m_roster.findClass( currentClassName() );
	if( item == nullptr ) { return; }
	registerComputers( item->students );
	const auto available = onlineIps();
	QStringList sorted = available;
	std::sort( sorted.begin(), sorted.end(), []( const QString& a, const QString& b ) {
		return QHostAddress( a ).toIPv4Address() < QHostAddress( b ).toIPv4Address();
	} );

	QVector<ClassroomStudent> updated;
	int automaticIndex = 0;
	for( const auto& student : item->students )
	{
		auto copy = student;
		if( copy.ip.isEmpty() && automaticIndex < sorted.size() )
		{
			copy.ip = sorted.at( automaticIndex++ );
		}
		if( copy.ip.isEmpty() == false )
		{
			pushIdentity( copy.ip, copy.seat, copy.name );
		}
		updated.append( copy );
	}

	auto* writable = m_roster.findClass( currentClassName() );
	if( writable ) { writable->students = updated; m_roster.save(); }
	refreshRosterTable();
	refreshStatus();
}


void ClassroomMasterDialog::quickLoginByIp()
{
	const auto ip = m_quickIp->text().trimmed();
	const auto seat = m_quickSeat->text().trimmed();
	const auto name = m_quickName->text().trimmed();
	if( ip.isEmpty() || seat.isEmpty() || name.isEmpty() )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Please enter the IP address, seat number and student name." ) );
		return;
	}
	ClassroomStudent student;
	student.seat = seat;
	student.name = name;
	student.ip = ip;
	registerComputers( { student } );
	pushSettings();
	pushKey();
	pushIdentity( ip, seat, name );
	refreshStatus();
}


void ClassroomMasterDialog::batchLogin()
{
	pushSettings();
	pushRoster();
	pushKey();
	QVector<ClassroomStudent> students;
	for( int row = 0; row < m_rosterTable->rowCount(); ++row )
	{
		ClassroomStudent student;
		student.seat = m_rosterTable->item( row, 0 ) ? m_rosterTable->item( row, 0 )->text().trimmed() : QString{};
		student.name = m_rosterTable->item( row, 1 ) ? m_rosterTable->item( row, 1 )->text().trimmed() : QString{};
		student.ip = m_rosterTable->item( row, 2 ) ? m_rosterTable->item( row, 2 )->text().trimmed() : QString{};
		students.append( student );
	}
	registerComputers( students );
	for( int row = 0; row < m_rosterTable->rowCount(); ++row )
	{
		const auto ip = m_rosterTable->item( row, 2 ) ? m_rosterTable->item( row, 2 )->text().trimmed() : QString{};
		const auto seat = m_rosterTable->item( row, 0 ) ? m_rosterTable->item( row, 0 )->text().trimmed() : QString{};
		const auto name = m_rosterTable->item( row, 1 ) ? m_rosterTable->item( row, 1 )->text().trimmed() : QString{};
		if( ip.isEmpty() == false ) { pushIdentity( ip, seat, name ); }
	}
	refreshStatus();
}


void ClassroomMasterDialog::batchLogout()
{
	for( const auto& ip : selectedIps() ) { pushLogout( ip ); }
	refreshStatus();
}


void ClassroomMasterDialog::forceLogout()
{
	const auto ips = selectedIps();
	if( ips.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please select a student row first." ) );
		return;
	}
	pushLogout( ips.first() );
	refreshStatus();
}


void ClassroomMasterDialog::switchStudent()
{
	const auto ips = selectedIps();
	if( ips.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please select a student row first." ) );
		return;
	}
	const auto* item = m_roster.findClass( currentClassName() );
	if( item == nullptr ) { return; }

	QStringList choices;
	for( const auto& student : item->students )
	{
		choices.append( QStringLiteral("%1 %2").arg( student.seat, student.name ) );
	}
	bool ok = false;
	const auto choice = QInputDialog::getItem( this, windowTitle(), tr( "New student:" ), choices, 0, false, &ok );
	if( ok == false ) { return; }
	const auto parts = choice.split( QLatin1Char(' '), Qt::SkipEmptyParts );
	if( parts.size() < 2 ) { return; }

	auto message = Classroom::message( Command::SwitchStudent );
	message.addArgument( Argument::ClassName, currentClassName() );
	message.addArgument( Argument::NewSeat, parts.value( 0 ) );
	message.addArgument( Argument::NewStudent, parts.value( 1 ) );
	message.addArgument( Argument::SessionId, currentSessionId() );
	sendToIp( ips.first(), message );
	appendTeacherEvent( EventType::switchUser(), tr( "teacher switched the student to %1" ).arg( choice ),
						ips.first(), parts.value( 0 ), parts.value( 1 ) );
	refreshStatus();
}


void ClassroomMasterDialog::lockAll( bool locked )
{
	auto message = Classroom::message( locked ? Command::LockClient : Command::UnlockClient );
	message.addArgument( Argument::Enabled, locked );
	sendToAll( message );
	appendTeacherEvent( locked ? EventType::lock() : EventType::unlock(),
						locked ? tr( "all clients locked" ) : tr( "all clients unlocked" ) );
}


void ClassroomMasterDialog::toggleAi( bool enabled )
{
	auto message = Classroom::message( Command::ToggleAi );
	message.addArgument( Argument::Enabled, enabled );
	sendToAll( message );
	appendTeacherEvent( EventType::connection(), enabled ? tr( "AI enabled for all clients" )
														: tr( "AI disabled for all clients" ) );
}


void ClassroomMasterDialog::setupLogTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );

	auto* filterRow = new QHBoxLayout;
	m_logFrom = new QDateEdit( page );
	m_logFrom->setCalendarPopup( true );
	m_logFrom->setDate( QDate::currentDate().addDays( -30 ) );
	m_logTo = new QDateEdit( page );
	m_logTo->setCalendarPopup( true );
	m_logTo->setDate( QDate::currentDate() );
	m_logScope = new QComboBox( page );
	m_logScope->addItems( { QStringLiteral("teacher"), QStringLiteral("client") } );
	m_logMachine = new QComboBox( page );
	m_logMachine->addItem( tr( "All computers" ) );
	m_logType = new QComboBox( page );
	m_logType->addItems( { tr( "All" ), QStringLiteral("login"), QStringLiteral("logout"), QStringLiteral("switch"),
						   QStringLiteral("app"), QStringLiteral("window"), QStringLiteral("web"), QStringLiteral("ai"),
						   QStringLiteral("key-failed"), QStringLiteral("quit-attempt"), QStringLiteral("lock") } );
	m_logKeyword = new QLineEdit( page );
	m_logKeyword->setPlaceholderText( tr( "Keyword" ) );
	filterRow->addWidget( new QLabel( tr( "From:" ), page ) );
	filterRow->addWidget( m_logFrom );
	filterRow->addWidget( new QLabel( tr( "To:" ), page ) );
	filterRow->addWidget( m_logTo );
	filterRow->addWidget( new QLabel( tr( "Source:" ), page ) );
	filterRow->addWidget( m_logScope );
	filterRow->addWidget( m_logMachine, 1 );
	filterRow->addWidget( m_logType );
	filterRow->addWidget( m_logKeyword, 1 );
	layout->addLayout( filterRow );

	auto* buttonRow = new QHBoxLayout;
	auto* refreshButton = new QPushButton( tr( "Refresh" ), page );
	auto* openButton = new QPushButton( tr( "Open per-machine logs" ), page );
	auto* exportButton = new QPushButton( tr( "Export CSV" ), page );
	auto* purgeButton = new QPushButton( tr( "Purge expired" ), page );
	buttonRow->addWidget( refreshButton );
	buttonRow->addWidget( openButton );
	buttonRow->addWidget( exportButton );
	buttonRow->addWidget( purgeButton );
	buttonRow->addStretch( 1 );
	layout->addLayout( buttonRow );

	connect( refreshButton, &QPushButton::clicked, this, [this]() { refreshMachines(); refreshLogs(); } );
	connect( openButton, &QPushButton::clicked, this, [this]() { openLogFolder(); } );
	connect( exportButton, &QPushButton::clicked, this, [this]() { exportLogs(); } );
	connect( purgeButton, &QPushButton::clicked, this, [this]() { purgeLogs(); } );
	connect( m_logScope, &QComboBox::currentTextChanged, this, [this]() { refreshMachines(); refreshLogs(); } );
	connect( m_logKeyword, &QLineEdit::returnPressed, this, [this]() { refreshLogs(); } );

	m_logTable = new QTableWidget( 0, 10, page );
	m_logTable->setHorizontalHeaderLabels( { tr( "Time" ), tr( "Class" ), tr( "Seat" ), tr( "Student" ),
											 tr( "Host" ), tr( "IP" ), tr( "Type" ), tr( "Interest" ),
											 tr( "Application" ), tr( "Detail" ) } );
	m_logTable->horizontalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
	m_logTable->horizontalHeader()->setStretchLastSection( true );
	m_logTable->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_logTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
	layout->addWidget( m_logTable, 1 );

	m_tabs->addTab( page, tr( "Activity logs" ) );
}


void ClassroomMasterDialog::refreshMachines()
{
	if( m_logMachine == nullptr ) { return; }
	const QSignalBlocker blocker( m_logMachine );
	const auto previous = m_logMachine->currentText();
	m_logMachine->clear();
	m_logMachine->addItem( tr( "All computers" ) );
	ClassroomLogStore store( dataDirectory(), m_logScope->currentText() );
	m_logMachine->addItems( store.machines() );
	if( previous.isEmpty() == false ) { m_logMachine->setCurrentText( previous ); }
}


QVector<ClassroomEvent> ClassroomMasterDialog::filteredEvents() const
{
	const auto scope = m_logScope ? m_logScope->currentText() : QStringLiteral("teacher");
	const auto machine = ( m_logMachine && m_logMachine->currentIndex() > 0 ) ? m_logMachine->currentText() : QString{};
	ClassroomLogStore store( dataDirectory(), scope );
	auto events = store.load( m_logFrom->date(), m_logTo->date(), machine );

	const auto categories = ClassroomInterestAnalyzer::categoriesFromJson(
		settingsJson().value( QStringLiteral("interestCategories") ).toString() );
	for( auto& event : events )
	{
		event.category = ClassroomInterestAnalyzer::categorize( event.detail + QLatin1Char(' ') + event.app, categories );
	}

	QVector<ClassroomEvent> result;
	const auto typeFilter = m_logType->currentText();
	const auto keyword = m_logKeyword->text().trimmed().toLower();
	for( const auto& event : events )
	{
		if( m_logType->currentIndex() > 0 && event.type != typeFilter ) { continue; }
		if( keyword.isEmpty() == false )
		{
			const QString searchable = event.detail + QLatin1Char(' ') + event.app +
									   QLatin1Char(' ') + event.student;
			if( searchable.toLower().contains( keyword ) == false ) { continue; }
		}
		result.append( event );
	}
	std::sort( result.begin(), result.end(), []( const ClassroomEvent& a, const ClassroomEvent& b ) {
		return a.timestamp > b.timestamp;
	} );
	return result;
}


void ClassroomMasterDialog::refreshLogs()
{
	m_logEvents = filteredEvents();
	const QSignalBlocker blocker( m_logTable );
	m_logTable->setRowCount( m_logEvents.size() );
	for( int row = 0; row < m_logEvents.size(); ++row )
	{
		const auto& event = m_logEvents.at( row );
		const QStringList values{ event.timestamp.toString( QStringLiteral("yyyy-MM-dd HH:mm:ss") ),
								 event.className, event.seat, event.student, event.host, event.ip,
								 event.type, event.category, event.app, event.detail };
		for( int column = 0; column < values.size(); ++column )
		{
			m_logTable->setItem( row, column, new QTableWidgetItem( values.at( column ) ) );
		}
	}
}


void ClassroomMasterDialog::openLogFolder()
{
	ClassroomLogStore store( dataDirectory(), m_logScope->currentText() );
	QDir().mkpath( store.machinesPath() );
	QDesktopServices::openUrl( QUrl::fromLocalFile( store.machinesPath() ) );
}


void ClassroomMasterDialog::exportLogs()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export activity logs" ),
													   QDir( dataDirectory() ).filePath( QStringLiteral("activity.csv") ),
													   tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() ) { return; }
	QString error;
	if( ClassroomLogStore::exportCsv( fileName, m_logEvents, &error ) == false )
	{
		QMessageBox::warning( this, windowTitle(), error );
	}
}


void ClassroomMasterDialog::purgeLogs()
{
	ClassroomLogStore store( dataDirectory(), m_logScope->currentText() );
	const auto removed = store.purgeExpired( settingsJson().value( QStringLiteral("retentionDays") ).toInt( 180 ) );
	QMessageBox::information( this, windowTitle(), tr( "%1 expired log files have been removed." ).arg( removed ) );
	refreshLogs();
}


void ClassroomMasterDialog::setupAnalysisTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );

	m_analysisHint = new QLabel( tr( "Profiles are computed from all lessons in the selected date range. "
									 "Recent behaviour is weighted more heavily so the suggested layer changes "
									 "dynamically over multiple lessons." ), page );
	m_analysisHint->setWordWrap( true );
	layout->addWidget( m_analysisHint );

	auto* controls = new QHBoxLayout;
	m_halfLife = new QSpinBox( page );
	m_halfLife->setRange( 1, 365 );
	m_halfLife->setValue( 30 );
	m_halfLife->setSuffix( tr( " days" ) );
	auto* refreshButton = new QPushButton( tr( "Refresh analysis" ), page );
	auto* editButton = new QPushButton( tr( "Edit interest keywords" ), page );
	auto* exportButton = new QPushButton( tr( "Export CSV" ), page );
	controls->addWidget( new QLabel( tr( "Half-life:" ), page ) );
	controls->addWidget( m_halfLife );
	controls->addWidget( refreshButton );
	controls->addWidget( editButton );
	controls->addWidget( exportButton );
	controls->addStretch( 1 );
	layout->addLayout( controls );

	connect( refreshButton, &QPushButton::clicked, this, [this]() { refreshAnalysis(); } );
	connect( editButton, &QPushButton::clicked, this, [this]() { editCategories(); } );
	connect( exportButton, &QPushButton::clicked, this, [this]() { exportAnalysis(); } );

	m_analysisTable = new QTableWidget( 0, 9, page );
	m_analysisTable->setHorizontalHeaderLabels( { tr( "Class" ), tr( "Seat" ), tr( "Student" ), tr( "Lessons" ),
												 tr( "Events" ), tr( "Leading interest" ), tr( "Interest mix" ),
												 tr( "Suggested layer" ), tr( "Trend" ) } );
	m_analysisTable->horizontalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
	m_analysisTable->horizontalHeader()->setStretchLastSection( true );
	m_analysisTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
	layout->addWidget( m_analysisTable, 1 );

	m_tabs->addTab( page, tr( "Interest analysis" ) );
}


void ClassroomMasterDialog::refreshAnalysis()
{
	const auto categories = ClassroomInterestAnalyzer::categoriesFromJson(
		settingsJson().value( QStringLiteral("interestCategories") ).toString() );
	ClassroomLogStore store( dataDirectory(), QStringLiteral("teacher") );
	const auto events = store.load( m_logFrom->date(), m_logTo->date() );
	m_profiles = ClassroomInterestAnalyzer::analyze( events, categories, m_halfLife->value() );

	const QSignalBlocker blocker( m_analysisTable );
	m_analysisTable->setRowCount( m_profiles.size() );
	for( int row = 0; row < m_profiles.size(); ++row )
	{
		const auto& profile = m_profiles.at( row );
		QVector<QPair<QString, double>> distribution;
		double total = 0.0;
		for( auto it = profile.scores.constBegin(); it != profile.scores.constEnd(); ++it )
		{
			distribution.append( { it.key(), it.value() } );
			total += it.value();
		}
		std::sort( distribution.begin(), distribution.end(),
				   []( const QPair<QString, double>& a, const QPair<QString, double>& b ) { return a.second > b.second; } );
		QStringList mix;
		for( int index = 0; index < qMin( 3, static_cast<int>( distribution.size() ) ); ++index )
		{
			const auto percent = total > 0.0 ? ( distribution.at( index ).second / total ) * 100.0 : 0.0;
			mix.append( QStringLiteral("%1 %2%").arg( distribution.at( index ).first ).arg( percent, 0, 'f', 0 ) );
		}

		const QStringList values{ profile.className, profile.seat, profile.student,
								  QString::number( profile.sessions ), QString::number( profile.events ),
								  profile.topCategory.isEmpty() ? tr( "insufficient data" ) : profile.topCategory,
								  mix.join( QStringLiteral(", ") ), profile.suggestion,
								  QStringLiteral("%1%").arg( profile.trend, 0, 'f', 0 ) };
		for( int column = 0; column < values.size(); ++column )
		{
			m_analysisTable->setItem( row, column, new QTableWidgetItem( values.at( column ) ) );
		}
	}
}


void ClassroomMasterDialog::editCategories()
{
	QDialog dialog( this );
	dialog.setWindowTitle( tr( "Interest keywords" ) );
	dialog.resize( 640, 480 );
	auto* editor = new QPlainTextEdit( &dialog );
	const auto current = settingsJson().value( QStringLiteral("interestCategories") ).toString();
	editor->setPlainText( current.isEmpty() ? ClassroomInterestAnalyzer::defaultCategoriesJson() : current );
	auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	auto* layout = new QVBoxLayout( &dialog );
	layout->addWidget( editor, 1 );
	layout->addWidget( buttons );
	connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	if( dialog.exec() != QDialog::Accepted ) { return; }
	const auto text = editor->toPlainText();
	if( QJsonDocument::fromJson( text.toUtf8() ).isArray() == false )
	{
		QMessageBox::warning( this, windowTitle(), tr( "The keyword configuration must be a valid JSON array." ) );
		return;
	}
	auto object = m_settings.object();
	object.insert( QStringLiteral("interestCategories"), text );
	m_settings.setObject( object );
	m_settings.save();
	refreshAnalysis();
	refreshLogs();
}


void ClassroomMasterDialog::exportAnalysis()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export interest profiles" ),
													   QDir( dataDirectory() ).filePath( QStringLiteral("profiles.csv") ),
													   tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() ) { return; }
	QVector<QStringList> rows;
	for( const auto& profile : m_profiles )
	{
		QStringList mix;
		for( auto it = profile.scores.constBegin(); it != profile.scores.constEnd(); ++it )
		{
			mix.append( QStringLiteral("%1=%2").arg( it.key() ).arg( it.value(), 0, 'f', 2 ) );
		}
		rows.append( { profile.className, profile.seat, profile.student, QString::number( profile.sessions ),
					   QString::number( profile.events ), profile.topCategory, mix.join( QStringLiteral("; ") ),
					   profile.suggestion, QString::number( profile.trend, 'f', 1 ) } );
	}
	QString error;
	if( ClassroomLogStore::exportProfilesCsv( fileName,
			{ tr( "Class" ), tr( "Seat" ), tr( "Student" ), tr( "Lessons" ), tr( "Events" ),
			  tr( "Leading interest" ), tr( "Interest mix" ), tr( "Suggested layer" ), tr( "Trend" ) },
			rows, &error ) == false )
	{
		QMessageBox::warning( this, windowTitle(), error );
	}
}


void ClassroomMasterDialog::setupAiTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );
	auto* form = new QFormLayout;
	m_aiEnabled = new QCheckBox( tr( "Enable AI assistant on student computers" ), page );
	m_aiEndpoint = new QLineEdit( page );
	m_aiEndpoint->setPlaceholderText( tr( "http://127.0.0.1:11434/v1/chat/completions" ) );
	m_aiApiKey = new QLineEdit( page );
	m_aiApiKey->setEchoMode( QLineEdit::Password );
	m_aiModel = new QLineEdit( page );
	m_aiSystemPrompt = new QPlainTextEdit( page );
	m_aiSystemPrompt->setMaximumHeight( 120 );
	m_aiTimeout = new QSpinBox( page );
	m_aiTimeout->setRange( 5, 600 );
	m_aiTimeout->setSuffix( tr( " s" ) );
	form->addRow( m_aiEnabled );
	form->addRow( tr( "Local or network API endpoint:" ), m_aiEndpoint );
	form->addRow( tr( "API key (optional):" ), m_aiApiKey );
	form->addRow( tr( "Model:" ), m_aiModel );
	form->addRow( tr( "System prompt:" ), m_aiSystemPrompt );
	form->addRow( tr( "Request timeout:" ), m_aiTimeout );
	layout->addLayout( form );

	auto* buttons = new QHBoxLayout;
	auto* saveButton = new QPushButton( tr( "Save on this computer" ), page );
	auto* pushButton = new QPushButton( tr( "Save and push to students" ), page );
	auto* testButton = new QPushButton( tr( "Test connection" ), page );
	buttons->addWidget( saveButton );
	buttons->addWidget( pushButton );
	buttons->addWidget( testButton );
	buttons->addStretch( 1 );
	layout->addLayout( buttons );

	m_aiTestResult = new QLabel( tr( "Not tested yet." ), page );
	m_aiTestResult->setWordWrap( true );
	layout->addWidget( m_aiTestResult );
	layout->addStretch( 1 );

	connect( saveButton, &QPushButton::clicked, this, [this]() { saveSettingsFromWidgets(); } );
	connect( pushButton, &QPushButton::clicked, this, [this]() { saveSettingsFromWidgets(); pushSettings(); } );
	connect( testButton, &QPushButton::clicked, this, [this]() { testAiConnection(); } );

	m_tabs->addTab( page, tr( "AI settings" ) );
}


void ClassroomMasterDialog::setupMonitorTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );
	auto* form = new QFormLayout;
	m_dataDirectory = new QLineEdit( page );
	m_monitoringEnabled = new QCheckBox( tr( "Record application, window title and browsing behaviour" ), page );
	m_captureWindowTitle = new QCheckBox( tr( "Record active window titles" ), page );
	m_captureProcess = new QCheckBox( tr( "Record active process names" ), page );
	m_exitProtection = new QCheckBox( tr( "Prevent students from exiting the client" ), page );
	m_exitRequiresAdmin = new QCheckBox( tr( "Exiting requires the administrator account" ), page );
	m_allowSelfLogin = new QCheckBox( tr( "Students may sign in themselves (seat + name + key)" ), page );
	m_exitAdminUsername = new QLineEdit( page );
	m_captureInterval = new QSpinBox( page );
	m_captureInterval->setRange( 1, 300 );
	m_captureInterval->setSuffix( tr( " s" ) );
	m_retentionDays = new QSpinBox( page );
	m_retentionDays->setRange( 1, 3650 );
	m_retentionDays->setSuffix( tr( " days" ) );
	form->addRow( tr( "Data directory:" ), m_dataDirectory );
	form->addRow( m_monitoringEnabled );
	form->addRow( m_captureWindowTitle );
	form->addRow( m_captureProcess );
	form->addRow( tr( "Sampling interval:" ), m_captureInterval );
	form->addRow( m_exitProtection );
	form->addRow( m_exitRequiresAdmin );
	form->addRow( tr( "Administrator account:" ), m_exitAdminUsername );
	form->addRow( m_allowSelfLogin );
	form->addRow( tr( "Log retention:" ), m_retentionDays );
	layout->addLayout( form );

	auto* buttons = new QHBoxLayout;
	auto* saveButton = new QPushButton( tr( "Save on this computer" ), page );
	auto* pushButton = new QPushButton( tr( "Save and push to students" ), page );
	auto* keyButton = new QPushButton( tr( "Generate and push key" ), page );
	auto* rosterButton = new QPushButton( tr( "Push roster" ), page );
	auto* openButton = new QPushButton( tr( "Open data directory" ), page );
	buttons->addWidget( saveButton );
	buttons->addWidget( pushButton );
	buttons->addWidget( keyButton );
	buttons->addWidget( rosterButton );
	buttons->addWidget( openButton );
	buttons->addStretch( 1 );
	layout->addLayout( buttons );
	layout->addStretch( 1 );

	connect( saveButton, &QPushButton::clicked, this, [this]() { saveSettingsFromWidgets(); } );
	connect( pushButton, &QPushButton::clicked, this, [this]() { saveSettingsFromWidgets(); pushSettings(); } );
	connect( keyButton, &QPushButton::clicked, this, [this]() { generateKey(); } );
	connect( rosterButton, &QPushButton::clicked, this, [this]() { pushRoster(); } );
	connect( openButton, &QPushButton::clicked, this, [this]() {
		QDir().mkpath( dataDirectory() );
		QDesktopServices::openUrl( QUrl::fromLocalFile( dataDirectory() ) );
	} );

	m_tabs->addTab( page, tr( "Monitoring & lock" ) );
}


void ClassroomMasterDialog::refreshSettingsWidgets()
{
	const auto object = settingsJson();
	m_aiEnabled->setChecked( object.value( SettingKey::aiEnabled() ).toBool( true ) );
	m_aiEndpoint->setText( object.value( SettingKey::aiEndpoint() ).toString() );
	m_aiApiKey->setText( object.value( SettingKey::aiApiKey() ).toString() );
	m_aiModel->setText( object.value( SettingKey::aiModel() ).toString() );
	m_aiSystemPrompt->setPlainText( object.value( SettingKey::aiSystemPrompt() ).toString() );
	m_aiTimeout->setValue( object.value( SettingKey::aiTimeout() ).toInt( 60 ) );

	m_dataDirectory->setText( object.value( SettingKey::dataDirectory() ).toString() );
	m_monitoringEnabled->setChecked( object.value( SettingKey::monitoringEnabled() ).toBool( true ) );
	m_captureWindowTitle->setChecked( object.value( SettingKey::captureWindowTitle() ).toBool( true ) );
	m_captureProcess->setChecked( object.value( SettingKey::captureProcess() ).toBool( true ) );
	m_exitProtection->setChecked( object.value( SettingKey::exitProtection() ).toBool( true ) );
	m_exitRequiresAdmin->setChecked( object.value( SettingKey::exitRequiresAdmin() ).toBool( true ) );
	m_allowSelfLogin->setChecked( object.value( SettingKey::allowSelfLogin() ).toBool( true ) );
	m_exitAdminUsername->setText( object.value( SettingKey::exitAdminUsername() ).toString() );
	m_captureInterval->setValue( object.value( SettingKey::captureInterval() ).toInt( 5 ) );
	m_retentionDays->setValue( object.value( QStringLiteral("retentionDays") ).toInt( 180 ) );
}


void ClassroomMasterDialog::saveSettingsFromWidgets()
{
	auto object = m_settings.object();
	object.insert( SettingKey::aiEnabled(), m_aiEnabled->isChecked() );
	object.insert( SettingKey::aiEndpoint(), m_aiEndpoint->text().trimmed() );
	object.insert( SettingKey::aiApiKey(), m_aiApiKey->text() );
	object.insert( SettingKey::aiModel(), m_aiModel->text().trimmed() );
	object.insert( SettingKey::aiSystemPrompt(), m_aiSystemPrompt->toPlainText() );
	object.insert( SettingKey::aiTimeout(), m_aiTimeout->value() );

	object.insert( SettingKey::dataDirectory(), m_dataDirectory->text().trimmed() );
	object.insert( SettingKey::monitoringEnabled(), m_monitoringEnabled->isChecked() );
	object.insert( SettingKey::captureWindowTitle(), m_captureWindowTitle->isChecked() );
	object.insert( SettingKey::captureProcess(), m_captureProcess->isChecked() );
	object.insert( SettingKey::exitProtection(), m_exitProtection->isChecked() );
	object.insert( SettingKey::exitRequiresAdmin(), m_exitRequiresAdmin->isChecked() );
	object.insert( SettingKey::allowSelfLogin(), m_allowSelfLogin->isChecked() );
	object.insert( SettingKey::exitAdminUsername(), m_exitAdminUsername->text().trimmed() );
	object.insert( SettingKey::captureInterval(), m_captureInterval->value() );
	object.insert( QStringLiteral("retentionDays"), m_retentionDays->value() );
	object.insert( SettingKey::bridgeEnabled(), m_bridgeEnabled->isChecked() );
	object.insert( SettingKey::bridgePort(), m_bridgePort->value() );
	object.insert( SettingKey::silencePageText(), m_silenceText->text() );
	m_settings.setObject( object );
	if( m_bridge ) { m_bridge->setSettings( settingsJson() ); }
	if( m_settings.save() == false )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Could not save %1" ).arg( m_settings.filePath() ) );
	}
}


void ClassroomMasterDialog::testAiConnection()
{
	ClassroomAiClient::Settings settings;
	settings.enabled = true;
	settings.endpoint = m_aiEndpoint->text().trimmed();
	settings.apiKey = m_aiApiKey->text();
	settings.model = m_aiModel->text().trimmed();
	settings.systemPrompt = m_aiSystemPrompt->toPlainText();
	settings.timeout = m_aiTimeout->value();
	m_testAiClient->setSettings( settings );
	m_aiTestResult->setText( tr( "Testing..." ) );
	m_testAiClient->chat( {}, QStringLiteral("ping") );
}


void ClassroomMasterDialog::showCall( const QString& ip, const QString& seat, const QString& student )
{
	if( m_scorePanel ) { m_scorePanel->showCall( ip, seat, student ); }
	if( m_systemStatus )
	{
		m_systemStatus->setText( tr( "Call from %1 (%2, seat %3)" ).arg( student, ip, seat ) );
	}
}


void ClassroomMasterDialog::syncNow()
{
	if( m_bridge == nullptr ) { return; }
	m_bridge->setSettings( settingsJson() );
	m_bridge->setClassName( currentClassName() );
	if( m_bridge->isRunning() == false )
	{
		m_bridge->start( m_bridgePort ? m_bridgePort->value() : 11880 );
	}
	m_bridge->syncNow();
	if( m_systemStatus ) { m_systemStatus->setText( m_bridge->status() ); }
}


void ClassroomMasterDialog::setupSeatTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );

	auto* toolbar = new QHBoxLayout;
	auto* checkInButton = new QPushButton( tr( "Start check-in" ), page );
	auto* clearButton = new QPushButton( tr( "Clear results" ), page );
	m_checkInSummary = new QLabel( tr( "No check-in started." ), page );
	m_checkInSummary->setWordWrap( true );
	toolbar->addWidget( checkInButton );
	toolbar->addWidget( clearButton );
	toolbar->addWidget( m_checkInSummary, 1 );
	layout->addLayout( toolbar );

	m_seatGrid = new ClassroomSeatGrid( m_roster );
	m_seatGrid->setClassName( currentClassName() );
	m_seatGrid->setThumbnailProvider( [this]( const QString& ip ) -> QImage {
		for( const auto& controlInterface : m_master.allComputerControlInterfaces() )
		{
			if( ipOfInterface( controlInterface.data() ) == ip )
			{
				return controlInterface->scaledFramebuffer();
			}
		}
		return {};
	} );
	m_seatGrid->setScoreProvider( [this]( const QString& seat ) -> int {
		return m_scorePanel ? m_scorePanel->scoreForSeat( seat ) : 100;
	} );
	m_seatGrid->setScoreSink( [this]( const QString& seat, int delta, const QString& reason ) {
		if( m_scorePanel )
		{
			m_scorePanel->addEntryForSeat( seat, delta,
										   reason.isEmpty() ? QString::number( delta ) : reason );
		}
		appendTeacherEvent( EventType::quickScore(),
							tr( "%1 points at seat %2: %3" ).arg( delta ).arg( seat, reason ), {} );
	} );

	auto* scroller = new QScrollArea( page );
	scroller->setWidgetResizable( true );
	scroller->setWidget( m_seatGrid );
	layout->addWidget( scroller, 3 );

	m_checkInTable = new QTableWidget( 0, 5, page );
	m_checkInTable->setHorizontalHeaderLabels( QStringList{ tr( "Seat" ), tr( "Roster name" ),
														  tr( "Typed name" ), tr( "Status" ), tr( "Time" ) } );
	m_checkInTable->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
	m_checkInTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_checkInTable->setMaximumHeight( 210 );
	layout->addWidget( m_checkInTable, 1 );

	connect( checkInButton, &QPushButton::clicked, this, &ClassroomMasterDialog::startCheckIn );
	connect( clearButton, &QPushButton::clicked, this, [this]() {
		m_checkInNames.clear();
		m_checkInTimes.clear();
		m_checkInStarted = {};
		refreshCheckIn();
	} );

	m_tabs->addTab( page, tr( "Seats & check-in" ) );
}


void ClassroomMasterDialog::refreshSeatGrid()
{
	if( m_seatGrid == nullptr ) { return; }
	m_seatGrid->setClassName( currentClassName() );
	m_seatGrid->setOnlineIps( onlineIps() );

	// Publish every student's current score to the matching Veyon Master
	// thumbnail. Both live in the same process, so a dynamic QObject property
	// is enough - no cross-module ABI change and no file I/O.
	const auto* classItem = m_roster.findClass( currentClassName() );
	if( classItem == nullptr || m_scorePanel == nullptr ) { return; }

	for( const auto& controlInterface : m_master.allComputerControlInterfaces() )
	{
		const auto ip = ipOfInterface( controlInterface.data() );
		for( const auto& student : classItem->students )
		{
			if( student.ip == ip )
			{
				controlInterface->setProperty( "classkeepScore",
											   m_scorePanel->scoreForSeat( student.seat ) );
				break;
			}
		}
	}
}


void ClassroomMasterDialog::startCheckIn()
{
	m_checkInNames.clear();
	m_checkInTimes.clear();
	m_checkInStarted = QDateTime::currentDateTime();

	auto message = Classroom::message( Command::StartCheckIn );
	message.addArgument( Argument::ClassName, currentClassName() );
	sendToAll( message );
	appendTeacherEvent( EventType::checkIn(),
						tr( "check-in started for %1" ).arg( currentClassName() ), {} );
	refreshCheckIn();
}


void ClassroomMasterDialog::recordCheckIn( const QString& className, const QString& seat,
										   const QString& typedName )
{
	if( seat.isEmpty() || className != currentClassName() ) { return; }
	m_checkInNames.insert( seat, typedName );
	m_checkInTimes.insert( seat, QDateTime::currentDateTime() );
	refreshCheckIn();
}


void ClassroomMasterDialog::refreshCheckIn()
{
	if( m_checkInTable == nullptr ) { return; }

	const auto* classItem = m_roster.findClass( currentClassName() );
	const int rows = classItem ? classItem->students.size() : 0;
	m_checkInTable->setRowCount( rows );
	int present = 0;
	int mismatched = 0;

	for( int row = 0; row < rows; ++row )
	{
		const auto& student = classItem->students.at( row );
		const auto typed = m_checkInNames.value( student.seat );
		QString status = tr( "absent" );
		if( typed.isEmpty() == false && typed == student.name )
		{
			status = tr( "present" );
			++present;
		}
		else if( typed.isEmpty() == false )
		{
			status = tr( "name mismatch" );
			++mismatched;
		}
		const auto time = m_checkInTimes.value( student.seat );

		m_checkInTable->setItem( row, 0, new QTableWidgetItem( student.seat ) );
		m_checkInTable->setItem( row, 1, new QTableWidgetItem( student.name ) );
		m_checkInTable->setItem( row, 2, new QTableWidgetItem( typed ) );
		m_checkInTable->setItem( row, 3, new QTableWidgetItem( status ) );
		m_checkInTable->setItem( row, 4, new QTableWidgetItem(
			time.isValid() ? time.toString( QStringLiteral("HH:mm:ss") ) : QString{} ) );
	}

	if( m_checkInSummary )
	{
		m_checkInSummary->setText(
			m_checkInStarted.isValid()
				? tr( "Check-in %1: %2 present, %3 name mismatch, %4 absent (total %5)" )
					  .arg( m_checkInStarted.toString( QStringLiteral("HH:mm:ss") ) )
					  .arg( present ).arg( mismatched ).arg( rows - present - mismatched ).arg( rows )
				: tr( "No check-in started." ) );
	}
}


void ClassroomMasterDialog::setupSystemTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );

	m_systemStatus = new QLabel( tr( "Ready." ), page );
	m_systemStatus->setWordWrap( true );
	layout->addWidget( m_systemStatus );

	auto* form = new QFormLayout;
	m_teacherIp = new QLineEdit( page );
	for( const auto& address : QNetworkInterface::allAddresses() )
	{
		if( address.isLoopback() == false && address.protocol() == QAbstractSocket::IPv4Protocol )
		{
			m_teacherIp->setText( address.toString() );
			break;
		}
	}
	m_teacherIp->setPlaceholderText( tr( "Teacher IP kept reachable while Internet is blocked" ) );
	m_packagePath = new QLineEdit( page );
	m_packagePath->setPlaceholderText( tr( "Installer path on the teacher machine" ) );
	m_silenceText = new QLineEdit( page );
	m_silenceText->setText( settingsJson().value( SettingKey::silencePageText() ).toString() );
	m_bridgeEnabled = new QCheckBox( tr( "Start mobile bridge" ), page );
	m_bridgeEnabled->setChecked( settingsJson().value( SettingKey::bridgeEnabled() ).toBool( false ) );
	m_bridgePort = new QSpinBox( page );
	m_bridgePort->setRange( 1024, 65535 );
	m_bridgePort->setValue( settingsJson().value( SettingKey::bridgePort() ).toInt( 11880 ) );
	form->addRow( tr( "Teacher IP:" ), m_teacherIp );
	form->addRow( tr( "Installer:" ), m_packagePath );
	form->addRow( tr( "Silence page:" ), m_silenceText );
	form->addRow( m_bridgeEnabled );
	form->addRow( tr( "Bridge port:" ), m_bridgePort );
	layout->addLayout( form );

	const auto addRow = [this, page, layout]( const QString& label, const QVector<QPair<QString, std::function<void()>>>& actions ) {
		auto* row = new QHBoxLayout;
		if( label.isEmpty() == false ) { row->addWidget( new QLabel( label, page ) ); }
		for( const auto& action : actions )
		{
			auto* button = new QPushButton( action.first, page );
			row->addWidget( button );
			connect( button, &QPushButton::clicked, this, action.second );
		}
		row->addStretch( 1 );
		layout->addLayout( row );
	};

	const auto send = [this]( Command command, bool enabled = true, const QString& text = QString{} ) {
		auto message = Classroom::message( command );
		message.addArgument( Argument::Enabled, enabled );
		message.addArgument( Argument::Ip, m_teacherIp->text().trimmed() );
		if( text.isEmpty() == false ) { message.addArgument( Argument::Message, text ); }
		sendToAll( message );
	};

	const auto feature = [this]( const QString& uid, bool start, const QVariantMap& arguments = QVariantMap{} ) {
		VeyonCore::featureManager().controlFeature( Feature::Uid{ uid },
			start ? FeatureProviderInterface::Operation::Start : FeatureProviderInterface::Operation::Stop,
			arguments, m_master.allComputerControlInterfaces() );
	};

	addRow( tr( "Audio:" ), {
		{ tr( "Mute" ), [send]() { send( Command::SetMute, true ); } },
		{ tr( "Unmute" ), [send]() { send( Command::SetMute, false ); } } } );
	addRow( tr( "Internet:" ), {
		{ tr( "Block" ), [send]() { send( Command::SetInternetBlock, true ); } },
		{ tr( "Allow" ), [send]() { send( Command::SetInternetBlock, false ); } } } );
	addRow( tr( "USB input:" ), {
		{ tr( "Block" ), [feature]() { feature( QStringLiteral("e4a77879-e544-4fec-bc18-e534f33b934c"), true ); } },
		{ tr( "Restore" ), [feature]() { feature( QStringLiteral("e4a77879-e544-4fec-bc18-e534f33b934c"), false ); } } } );
	addRow( tr( "Silence page:" ), {
		{ tr( "Show" ), [this, send]() { send( Command::ShowSilencePage, true, m_silenceText->text() ); } },
		{ tr( "Hide" ), [send]() { send( Command::HideSilencePage, false ); } } } );
	addRow( tr( "Screen:" ), {
		{ tr( "Remote control" ), [feature]() { feature( QStringLiteral("ca00ad68-1709-4abe-85e2-48dff6ccf8a2"), true ); } },
		{ tr( "Group demo" ), [feature]() { feature( QStringLiteral("7b6231bd-eb89-45d3-af32-f70663b2f878"), true ); } },
		{ tr( "Broadcast" ), [feature]() { feature( QStringLiteral("2aca1e9f-25f9-4d0f-9729-01b03c80ab28"), true ); } } } );
	addRow( tr( "UWF:" ), {
		{ tr( "Query" ), [this]() {
			sendToAll( Classroom::message( Command::UwfQuery ) );
			const auto state = ClassroomSystem::uwfState();
			m_systemStatus->setText( tr( "Local UWF: supported=%1 enabled=%2; client results are in the activity log" )
									 .arg( state.supported ? tr( "yes" ) : tr( "no" ),
										   state.currentEnabled ? tr( "yes" ) : tr( "no" ) ) );
		} },
		{ tr( "Disable + reboot" ), [this]() {
			sendToAll( Classroom::message( Command::UwfSetFilter ) );
			sendToAll( Classroom::message( Command::UwfRestart ) );
			m_systemStatus->setText( tr( "UWF disable queued on all clients; reboot sent." ) );
		} },
		{ tr( "Enable + reboot" ), [this]() {
			auto message = Classroom::message( Command::UwfSetFilter );
			message.addArgument( Argument::Enabled, true );
			sendToAll( message );
			sendToAll( Classroom::message( Command::UwfRestart ) );
			m_systemStatus->setText( tr( "UWF enable queued on all clients; reboot sent." ) );
		} } } );
	addRow( tr( "Uninstall protection:" ), {
		{ tr( "Enable on all students" ), [this]() {
			auto message = Classroom::message( Command::SetUninstallProtection );
			message.addArgument( Argument::Enabled, true );
			sendToAll( message );
			m_systemStatus->setText( tr( "Student uninstall protection enabled." ) );
		} },
		{ tr( "Release" ), [this]() {
			auto message = Classroom::message( Command::SetUninstallProtection );
			message.addArgument( Argument::Enabled, false );
			sendToAll( message );
			m_systemStatus->setText( tr( "Student uninstall protection released." ) );
		} } } );
	addRow( tr( "Deploy:" ), {
		{ tr( "Distribute and install" ), [this, feature]() {
			const auto path = m_packagePath->text().trimmed();
			if( path.isEmpty() )
			{
				QMessageBox::information( this, windowTitle(), tr( "Please choose an installer path first." ) );
				return;
			}
			const auto command = ClassroomSystem::softwareInstallCommand( path );
			feature( QStringLiteral("da9ca56a-b2ad-4fff-8f8a-929b2927b442"), true,
					 QVariantMap{ { QStringLiteral("Applications"), QStringList{ command } } } );
			m_systemStatus->setText( tr( "Install command sent: %1" ).arg( command ) );
		} },
		{ tr( "Refresh status" ), [this]() { sendToAll( Classroom::message( Command::UwfQuery ) ); } } } );
	addRow( tr( "Mobile:" ), {
		{ tr( "Sync now" ), [this]() { syncNow(); } },
		{ tr( "Stop bridge" ), [this]() {
			if( m_bridge ) { m_bridge->stop(); m_systemStatus->setText( m_bridge->status() ); }
		} } } );

	layout->addStretch( 1 );
	m_bridge = new ClassroomBridge( dataDirectory(), m_roster, m_store, settingsJson(), m_master, this );
	if( m_bridgeEnabled->isChecked() )
	{
		m_bridge->start( m_bridgePort->value() );
		m_systemStatus->setText( m_bridge->status() );
	}
	m_tabs->addTab( page, tr( "System & devices" ) );
}
