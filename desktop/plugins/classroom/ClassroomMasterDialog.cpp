/*
 * ClassroomMasterDialog.cpp - teacher console for classes, logs, analysis and AI
 *
 * This file is part of the Ikeback classroom modification.
 */

#include <algorithm>
#include <functional>

#include <QAbstractItemView>
#include <QAbstractSocket>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
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
#include <QJsonValue>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPair>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTcpSocket>
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
#include "ClassroomScore.h"
#include "ClassroomScorePanel.h"
#include "ClassroomSeatGrid.h"
#include "ClassroomSecrets.h"
#include "ClassroomSystem.h"
#include "ComputerControlInterface.h"
#include "FeatureManager.h"
#include "FeatureProviderInterface.h"
#include "Filesystem.h"
#include "NetworkObject.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "IkebackConfiguration.h"
#include "IkebackCore.h"
#include "IkebackMasterInterface.h"

using namespace Classroom;


ClassroomMasterDialog::ClassroomMasterDialog( IkebackMasterInterface& master,
											 ClassroomConfiguration& configuration,
											 ClassroomRoster& roster,
											 ClassroomLogStore& store,
											 ClassroomSettings& settings,
											 QWidget* parent ) :
	QWidget( parent ),
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
	setupTimetableTab();
	// Created last so it can reuse widgets from the other tabs, but inserted as
	// the FIRST tab: these are the actions a teacher uses every lesson.
	setupToolsTab();
	m_tabs->setCurrentIndex( 0 );

	auto* closeButton = new QPushButton( tr( "Hide panel" ), this );
	connect( closeButton, &QPushButton::clicked, this, [this]() { Q_EMIT hideRequested(); } );
	auto* footer = new QHBoxLayout;
	footer->addStretch( 1 );
	footer->addWidget( closeButton );

	auto* layout = new QVBoxLayout( this );
	layout->addWidget( m_tabs, 1 );
	layout->addLayout( footer );

	// Run in the teacher's interactive account: DPAPI blobs are bound to this
	// user, so the one-time enc: -> dpapi: migration must happen here.
	if( m_settings.migrateSecrets() == false )
	{
		qWarning( "ikeback: could not migrate legacy settings.json secrets" );
	}
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
	return IkebackCore::filesystem().expandPath( configured );
}


QJsonObject ClassroomMasterDialog::settingsJson() const
{
	return m_settings.object();
}


QJsonObject ClassroomMasterDialog::settingsForStudents() const
{
	return ClassroomSettings::forStudents( settingsJson() );
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
	auto* importButton = new QPushButton( tr( "Import table" ), page );
	importButton->setToolTip( tr( "Import a CSV/Excel table: class, seat number, student name and IP. "
								   "Multiple classes can be imported at once." ) );
	auto* pasteButton = new QPushButton( tr( "Paste from Excel" ), page );
	pasteButton->setToolTip( tr( "Copy the rows in Excel/WPS (class, seat, name, IP) and paste them here" ) );
	auto* templateButton = new QPushButton( tr( "Template" ), page );
	templateButton->setToolTip( tr( "Save a ready-to-fill CSV template" ) );
	auto* exportButton = new QPushButton( tr( "Export CSV" ), page );
	auto* saveRosterButton = new QPushButton( tr( "Save roster" ), page );
	classRow->addWidget( addClassButton );
	classRow->addWidget( importButton );
	classRow->addWidget( pasteButton );
	classRow->addWidget( templateButton );
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
	connect( pasteButton, &QPushButton::clicked, this, [this]() { importRosterPaste(); } );
	connect( templateButton, &QPushButton::clicked, this, [this]() { saveImportTemplate(); } );
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
	QMap<QString, QString> seatByIp;
	QSet<QString> seats;
	for( int row = 0; row < m_rosterTable->rowCount(); ++row )
	{
		ClassroomStudent student;
		student.seat = m_rosterTable->item( row, 0 ) ? m_rosterTable->item( row, 0 )->text().trimmed() : QString{};
		student.name = m_rosterTable->item( row, 1 ) ? m_rosterTable->item( row, 1 )->text().trimmed() : QString{};
		student.ip = m_rosterTable->item( row, 2 ) ? m_rosterTable->item( row, 2 )->text().trimmed() : QString{};
		if( student.seat.isEmpty() && student.name.isEmpty() ) { continue; }

		// Two seats on one machine is a data error: any identity push to that IP
		// would overwrite the other student. Refuse instead of guessing.
		if( student.ip.isEmpty() == false )
		{
			if( seatByIp.contains( student.ip ) )
			{
				QMessageBox::warning( this, windowTitle(),
									  tr( "IP %1 is used by seat %2 and seat %3 in class %4. "
										  "Each student needs a different machine." )
										  .arg( student.ip, seatByIp.value( student.ip ), student.seat,
												currentClassName() ) );
				return;
			}
			seatByIp.insert( student.ip, student.seat );
		}
		if( student.seat.isEmpty() == false )
		{
			if( seats.contains( student.seat ) )
			{
				QMessageBox::warning( this, windowTitle(),
									  tr( "Seat %1 appears twice in class %2." )
										  .arg( student.seat, currentClassName() ) );
				return;
			}
			seats.insert( student.seat );
		}
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

	auto& configuration = IkebackCore::config();
	if( configuration.isStoreWritable() == false )
	{
		QMessageBox::information( this, windowTitle(),
								  tr( "ikeback Master cannot write the global computer directory. "
									  "Run it as administrator or use the ikeback Configurator. "
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
	if( auto* directory = IkebackCore::networkObjectDirectoryManager().configuredDirectory() )
	{
		directory->update();
	}
}


void ClassroomMasterDialog::importRoster()
{
	const auto fileName = QFileDialog::getOpenFileName(
		this, tr( "Import class table" ), {},
		tr( "Tables (*.csv *.txt *.tsv);;CSV files (*.csv);;All files (*)" ) );
	if( fileName.isEmpty() ) { return; }

	QString error;
	ClassroomImportReport report;
	if( m_roster.importTableFile( fileName, currentClassName(), &report, &error ) == false )
	{
		QMessageBox::warning( this, windowTitle(),
							  error.isEmpty() ? tr( "Nothing could be imported from that file." ) : error );
		return;
	}
	m_roster.save();
	refreshRosterTable();
	pushRoster();
	if( const auto* imported = m_roster.findClass( currentClassName() ) )
	{
		registerComputers( imported->students );
	}
	reportImport( report );
}


void ClassroomMasterDialog::importRosterPaste()
{
	QDialog dialog( this );
	dialog.setWindowTitle( tr( "Paste students from Excel" ) );
	auto* layout = new QVBoxLayout( &dialog );
	auto* hint = new QLabel( tr( "Select the rows in Excel/WPS and copy them (Ctrl+C), then paste here.\n"
								 "Columns: class, seat number, student name, IP address. "
								 "The class column is optional - without it the current class is used." ),
							 &dialog );
	hint->setWordWrap( true );
	layout->addWidget( hint );

	auto* editor = new QPlainTextEdit( &dialog );
	editor->setPlaceholderText( tr( "3年级1班\t1\t张三\t192.168.1.101" ) );
	editor->setMinimumSize( 520, 260 );
	layout->addWidget( editor, 1 );

	auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	layout->addWidget( buttons );
	connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
	connect( editor, &QPlainTextEdit::textChanged, &dialog, [&dialog, editor, buttons]() {
		buttons->button( QDialogButtonBox::Ok )->setEnabled( editor->toPlainText().trimmed().isEmpty() == false );
	} );
	buttons->button( QDialogButtonBox::Ok )->setEnabled( false );

	if( dialog.exec() != QDialog::Accepted ) { return; }
	const auto text = editor->toPlainText();
	if( text.trimmed().isEmpty() ) { return; }

	ClassroomImportReport report;
	if( m_roster.importTable( text, currentClassName(), &report ) == false )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Nothing could be imported from the pasted text." ) );
		return;
	}
	m_roster.save();
	refreshRosterTable();
	pushRoster();
	if( const auto* imported = m_roster.findClass( currentClassName() ) )
	{
		registerComputers( imported->students );
	}
	reportImport( report );
}


void ClassroomMasterDialog::saveImportTemplate()
{
	const auto fileName = QFileDialog::getSaveFileName(
		this, tr( "Save import template" ),
		QDir( dataDirectory() ).filePath( QStringLiteral("ikeback-students-template.csv") ),
		tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() ) { return; }

	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Could not write %1" ).arg( fileName ) );
		return;
	}
	file.write( ClassroomRoster::importTemplate() );
	file.close();
	QMessageBox::information( this, windowTitle(),
							  tr( "Template saved to %1. Fill it in with Excel/WPS, then use "
								  "\"Import table\" or \"Paste from Excel\"." ).arg( fileName ) );
}


void ClassroomMasterDialog::reportImport( const ClassroomImportReport& report )
{
	QString text = tr( "Imported %1 students into %2 classes." )
					   .arg( report.students ).arg( qMax( 1, report.classes ) );
	if( report.overwritten > 0 )
	{
		text += QLatin1Char('\n') + tr( "%1 existing seats were updated." ).arg( report.overwritten );
	}
	if( report.ipConflicts > 0 )
	{
		text += QLatin1Char('\n') + tr( "%1 rows were skipped: that IP is already used by another seat "
										"in the same class." ).arg( report.ipConflicts );
	}
	if( report.problems.isEmpty() == false )
	{
		text += QLatin1Char('\n') + QLatin1Char('\n') + report.problems.join( QLatin1Char('\n') );
		QMessageBox::warning( this, windowTitle(), text );
	}
	else
	{
		QMessageBox::information( this, windowTitle(), text );
	}
}


bool ClassroomMasterDialog::upsertStudent( const QString& className, const QString& seat,
										   const QString& name, const QString& ip )
{
	if( className.isEmpty() || name.isEmpty() ) { return false; }

	auto* item = m_roster.findClass( className );
	if( item == nullptr )
	{
		ClassroomClass newClass;
		newClass.name = className;
		m_roster.classes().append( newClass );
		item = m_roster.findClass( className );
	}
	if( item == nullptr ) { return false; }

	// The same machine (IP) must not host two students of the same class: the
	// teacher would otherwise push two conflicting identities to one worker.
	if( ip.isEmpty() == false )
	{
		for( const auto& existing : item->students )
		{
			if( existing.ip == ip && existing.seat != seat )
			{
				QMessageBox::warning( this, windowTitle(),
									  tr( "IP %1 is already used by seat %2 (%3) in class %4." )
										  .arg( ip, existing.seat, existing.name, className ) );
				return false;
			}
		}
	}

	for( auto& existing : item->students )
	{
		if( existing.seat == seat && seat.isEmpty() == false )
		{
			existing.name = name;
			existing.ip = ip;
			return true;
		}
	}
	ClassroomStudent student;
	student.seat = seat;
	student.name = name;
	student.ip = ip;
	item->students.append( student );
	return true;
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
							  tr( "A new lesson has been started. A new mobile bridge key was generated; "
								  "settings and roster were pushed to all computers." ) );
}


void ClassroomMasterDialog::generateKey()
{
	auto object = m_settings.object();
	// 128 bit from the system CSP, hex encoded. This key authorises the mobile
	// bridge only; it is NEVER sent to a student machine.
	object.insert( QStringLiteral("sessionKey"), ClassroomSecrets::generateBridgeKey() );
	m_settings.setObject( object );
	m_settings.save();
	if( m_bridge ) { m_bridge->setSettings( settingsJson() ); }
	refreshStatus();
	pushKey();
	appendTeacherEvent( EventType::lock(), tr( "new mobile bridge key generated" ) );
}


void ClassroomMasterDialog::pushSettings()
{
	auto message = Classroom::message( Command::SetSettings );
	message.addArgument( Argument::SettingsJson,
						 QString::fromUtf8( QJsonDocument( settingsForStudents() ).toJson( QJsonDocument::Compact ) ) );
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
	// Kept as a heartbeat, but the wire now carries a boolean only. A student
	// machine must never see the key that authorises the mobile bridge.
	auto message = Classroom::message( Command::SetSessionKey );
	message.addArgument( Argument::KeyConfigured, sessionKey().isEmpty() == false );
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

	// The typed student must end up in the roster table, otherwise the teacher
	// signs someone in and the class list still looks empty.
	if( upsertStudent( currentClassName(), seat, name, ip ) == false ) { return; }
	m_roster.save();
	refreshRosterTable();

	ClassroomStudent student;
	student.seat = seat;
	student.name = name;
	student.ip = ip;
	registerComputers( { student } );
	pushSettings();
	pushKey();
	pushIdentity( ip, seat, name );
	refreshStatus();

	m_quickName->clear();
	setToolStatus( tr( "%1 (seat %2) added to %3 and signed in on %4." )
					   .arg( name, seat, currentClassName(), ip ) );
	QMessageBox::information( this, windowTitle(),
							  tr( "%1 (seat %2) was added to %3 and signed in on %4." )
								  .arg( name, seat, currentClassName(), ip ) );
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
	m_allowSelfLogin = new QCheckBox( tr( "Students may sign in themselves (seat + name)" ), page );
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
	auto* keyButton = new QPushButton( tr( "Generate new mobile bridge key" ), page );
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


void ClassroomMasterDialog::runConnectivityCheck()
{
	if( m_connectivityLabel == nullptr ) { return; }

	if( m_bridgeEnabled && m_bridge && m_bridge->isRunning() == false )
	{
		syncNow();
	}

	const auto serverPort = IkebackCore::config().ikebackServerPort() + int( IkebackCore::sessionId() );
	QTcpSocket socket;
	socket.connectToHost( QHostAddress::LocalHost, static_cast<quint16>( serverPort ) );
	const auto serverReachable = socket.waitForConnected( 1500 );
	socket.abort();

	QStringList lines;
	if( serverReachable )
	{
		lines << tr( "This computer: ikeback Server is reachable on 127.0.0.1:%1 - the red icon should "
					 "switch to green within a few seconds." ).arg( serverPort );
	}
	else
	{
		lines << tr( "This computer: NOTHING is listening on 127.0.0.1:%1. The ikeback service/server is not "
					 "running, which is exactly why this computer shows the red icon and cannot be controlled. "
					 "Press \"Restart ikeback service\"; if it still fails, read install.log in the ikeback "
					 "install directory." ).arg( serverPort );
	}

	if( m_bridge != nullptr )
	{
		lines << m_bridge->status();
		if( m_bridge->isRunning() )
		{
			lines << tr( "Phone: enter IP %1 and port %2 (same Wi-Fi/LAN, no client isolation)." )
					 .arg( m_teacherIp != nullptr ? m_teacherIp->text().trimmed() : QString{},
						   QString::number( m_bridgePort != nullptr ? m_bridgePort->value() : 11880 ) );
		}
		else
		{
			lines << tr( "Mobile bridge is not running: tick \"Start mobile bridge\" and press \"Sync now\"." );
		}
	}

	lines << tr( "If OTHER computers cannot connect: allow ikeback in Windows Firewall for the private "
				 "network, and make sure teacher and students are on the same subnet." );

	m_connectivityLabel->setText( lines.join( QLatin1Char('\n') ) );
}


void ClassroomMasterDialog::restartService()
{
	const auto cli = QDir( QCoreApplication::applicationDirPath() )
						 .filePath( QStringLiteral("ikeback-wcli.exe") );
	if( QFileInfo::exists( cli ) == false )
	{
		QMessageBox::warning( this, windowTitle(),
							  tr( "ikeback-wcli.exe was not found next to the Master. "
								  "Please reinstall ikeback." ) );
		return;
	}

	if( QProcess::startDetached( cli, { QStringLiteral("service"), QStringLiteral("restart") } ) == false )
	{
		QMessageBox::warning( this, windowTitle(),
							  tr( "The service could not be restarted. Run ikeback Master as administrator, "
								  "or restart the computer." ) );
		return;
	}

	QMessageBox::information( this, windowTitle(),
							  tr( "Restart requested. Wait a few seconds, then press \"Check connection\" again." ) );
}


void ClassroomMasterDialog::syncNow()
{
	if( m_bridge == nullptr ) { return; }

	if( m_bridgeEnabled != nullptr && m_bridgeEnabled->isChecked() == false )
	{
		if( m_bridge->isRunning() ) { m_bridge->stop(); }
		if( m_systemStatus != nullptr )
		{
			m_systemStatus->setText( tr( "Mobile bridge is switched off. Tick \"Start mobile bridge\" "
										 "to let phones connect." ) );
		}
		return;
	}

	m_bridge->setSettings( settingsJson() );
	m_bridge->setClassName( currentClassName() );

	const auto address = QHostAddress( m_teacherIp ? m_teacherIp->text().trimmed() : QString{} );
	const auto port = m_bridgePort ? m_bridgePort->value() : 11880;
	// Switching the adapter or the port in the UI must actually rebind, instead
	// of leaving the bridge on the old address where no phone can reach it.
	if( m_bridge->isRunning() &&
		( m_bridge->port() != port ||
		  ( address.isNull() == false && m_bridge->bindAddress() != address ) ) )
	{
		m_bridge->stop();
	}
	if( m_bridge->isRunning() == false )
	{
		if( m_bridge->start( port, address ) == false )
		{
			QMessageBox::warning( this, windowTitle(),
								  tr( "The mobile bridge could not be started on %1:%2. Pick another "
									  "adapter or port, and allow ikeback in the firewall." )
									  .arg( address.isNull() ? tr( "this computer" ) : address.toString() )
									  .arg( port ) );
		}
	}
	m_bridge->syncNow();
	if( m_systemStatus ) { m_systemStatus->setText( m_bridge->status() ); }
}


QStringList ClassroomMasterDialog::toolTargets() const
{
	if( m_toolScope && m_toolScope->currentIndex() == 1 ) { return selectedIps(); }
	return onlineIps();
}


void ClassroomMasterDialog::sendToTargets( const FeatureMessage& message )
{
	if( m_toolScope && m_toolScope->currentIndex() == 1 )
	{
		for( const auto& ip : selectedIps() ) { sendToIp( ip, message ); }
	}
	else
	{
		sendToAll( message );
	}
}


void ClassroomMasterDialog::setToolStatus( const QString& text )
{
	if( m_toolsStatus ) { m_toolsStatus->setText( text ); }
	if( m_systemStatus ) { m_systemStatus->setText( text ); }
}


void ClassroomMasterDialog::setClientProtection( bool enabled )
{
	// CLIENT protection: stop the student from quitting the ikeback worker /
	// floating card. Completely unrelated to UWF or the uninstall guard.
	auto message = Classroom::message( Command::SetExitProtection );
	message.addArgument( Argument::Enabled, enabled );
	sendToAll( message );

	auto object = settingsJson();
	object.insert( SettingKey::exitProtection(), enabled );
	m_settings.setObject( object );
	m_settings.save();
	if( m_exitProtection ) { m_exitProtection->setChecked( enabled ); }

	setToolStatus( enabled ?
		tr( "Client protection ON: students can no longer quit the ikeback card." ) :
		tr( "Client protection OFF: the card may now be closed (use only for maintenance)." ) );
	appendTeacherEvent( EventType::protection(),
						enabled ? tr( "client protection enabled" ) : tr( "client protection disabled" ) );
}


void ClassroomMasterDialog::setUwfFilter( bool enabled )
{
	// UWF restore: OS-level write filter. Disabling it is what you do BEFORE
	// installing software; enabling it makes the machine revert on reboot and
	// protects against a student machine "dropping out" after changes.
	// Deliberately does NOT reboot: the teacher may need to install right now.
	auto message = Classroom::message( Command::UwfSetFilter );
	message.addArgument( Argument::Enabled, enabled );
	sendToAll( message );

	setToolStatus( enabled ?
		tr( "UWF restore queued for all students. Takes effect after reboot; use Restart students if needed." ) :
		tr( "UWF restore disabled for all students. Install your software now, then enable it again." ) );
	appendTeacherEvent( EventType::uwf(),
						enabled ? tr( "UWF restore enabled" ) : tr( "UWF restore disabled" ) );
}


void ClassroomMasterDialog::applyFuzzyRules( const QString& key, const QString& text )
{
	QStringList rules;
	for( const auto& line : text.split( QLatin1Char('\n') ) )
	{
		for( const auto& part : line.split( QLatin1Char(',') ) )
		{
			const auto rule = part.trimmed();
			if( rule.isEmpty() == false ) { rules.append( rule ); }
		}
	}

	QJsonArray array;
	for( const auto& rule : rules ) { array.append( rule ); }

	auto object = settingsJson();
	object.insert( key, array );
	m_settings.setObject( object );
	m_settings.save();

	auto message = Classroom::message( key == SettingKey::blockedApps() ?
									   Command::SetAppBlockRules : Command::SetWebBlockRules );
	message.addArgument( Argument::RulesJson,
						 QString::fromUtf8( QJsonDocument( array ).toJson( QJsonDocument::Compact ) ) );
	sendToAll( message );

	setToolStatus( tr( "%1 fuzzy rule(s) applied: %2" )
				   .arg( rules.size() ).arg( rules.isEmpty() ? tr( "(none - blocking cleared)" ) : rules.join( QStringLiteral(", ") ) ) );
}


void ClassroomMasterDialog::openOnTargets( bool website, const QString& target )
{
	const auto value = target.trimmed();
	if( value.isEmpty() )
	{
		setToolStatus( website ? tr( "Enter a website address first." ) : tr( "Enter a software path first." ) );
		return;
	}

	auto message = Classroom::message( website ? Command::OpenWebsite : Command::OpenSoftware );
	if( website )
	{
		message.addArgument( Argument::Message, value );
	}
	else
	{
		message.addArgument( Argument::SoftwarePath, value );
		message.addArgument( Argument::SoftwareArgs, QString{} );
	}
	sendToTargets( message );
	setToolStatus( tr( "Sent to %1 machine(s): %2" ).arg( toolTargets().size() ).arg( value ) );
}


void ClassroomMasterDialog::scoreWholeClass( int delta )
{
	const auto* item = m_roster.findClass( currentClassName() );
	if( item == nullptr || m_scorePanel == nullptr ) { return; }

	const auto deltaText = delta > 0 ? QStringLiteral("+%1").arg( delta ) : QString::number( delta );
	const auto label = tr( "Whole class %1" ).arg( deltaText );
	for( const auto& student : item->students )
	{
		m_scorePanel->addEntryForSeat( student.seat, delta, label );
	}
	appendTeacherEvent( EventType::quickScore(),
						tr( "whole class %1 points for %2 students" ).arg( deltaText ).arg( item->students.size() ) );
	setToolStatus( tr( "%1 points applied to all %2 students." ).arg( deltaText ).arg( item->students.size() ) );
	refreshSeatGrid();
}


void ClassroomMasterDialog::refreshUwfStatus()
{
	const auto events = m_store.load( QDate::currentDate(), QDate::currentDate() );
	QMap<QString, QString> byIp;
	for( const auto& event : events )
	{
		if( event.type == EventType::uwf() && byIp.contains( event.ip ) == false )
		{
			byIp.insert( event.ip, event.detail );
		}
	}
	const auto* item = m_roster.findClass( currentClassName() );
	m_uwfStatus.clear();
	if( item == nullptr ) { return; }
	for( const auto& student : item->students )
	{
		if( byIp.contains( student.ip ) ) { m_uwfStatus.insert( student.seat, byIp.value( student.ip ) ); }
	}
}


QString ClassroomMasterDialog::timetableFilePath() const
{
	return QDir( dataDirectory() ).filePath( QStringLiteral("timetable.json") );
}


void ClassroomMasterDialog::refreshLessonRecords()
{
	if( m_lessonRecords == nullptr ) { return; }

	// Natural per-class record: what was taught (score book lessons), what
	// happened (today's activity log) and how the students did (score entries).
	ClassroomScoreBook book( dataDirectory() );
	book.load( currentClassName() );

	QString text;
	const auto& lessons = book.lessons();
	for( auto index = lessons.size() - 1; index >= 0; --index )
	{
		const auto& lesson = lessons.at( index );
		QVector<ClassroomScoreEntry> entries;
		int totalDelta = 0;
		for( const auto& entry : book.entries() )
		{
			if( entry.lessonId == lesson.id )
			{
				entries.append( entry );
				totalDelta += entry.delta;
			}
		}
		text += tr( "%1 - lesson content: %2\n" )
					.arg( lesson.date.toString( Qt::ISODate ) )
					.arg( lesson.topic.isEmpty() ? tr( "(not filled in)" ) : lesson.topic );
		text += tr( "  Performance: %1 score entries, total %2%3 points\n" )
					.arg( int( entries.size() ) )
					.arg( totalDelta > 0 ? QStringLiteral("+") : QString{} )
					.arg( totalDelta );
		for( const auto& entry : entries )
		{
			text += tr( "  - %1 %2 (%3)\n" )
						.arg( entry.student.isEmpty() ? entry.seat : entry.student )
						.arg( entry.delta > 0 ? QStringLiteral("+%1").arg( entry.delta ) : QString::number( entry.delta ) )
						.arg( entry.label );
		}
		text += QLatin1Char('\n');
	}

	const auto events = m_store.load( QDate::currentDate(), QDate::currentDate() );
	if( events.isEmpty() == false )
	{
		text += tr( "What happened today (recent activity):\n" );
		const int first = qMax( 0, int( events.size() ) - 12 );
		for( int index = int( events.size() ) - 1; index >= first; --index )
		{
			const auto& event = events.at( index );
			text += tr( "  %1 - %2 - %3\n" )
						.arg( event.timestamp.toString( QStringLiteral("HH:mm") ) )
						.arg( event.type )
						.arg( event.detail );
		}
	}
	if( text.isEmpty() ) { text = tr( "No lesson records yet. Start a new lesson or score a student on the Score & reports tab." ); }
	m_lessonRecords->setPlainText( text );

	if( m_timetableSummary != nullptr )
	{
		QFile file( timetableFilePath() );
		QString summary = tr( "Timetable not uploaded by the phone yet." );
		if( file.open( QFile::ReadOnly ) )
		{
			const auto document = QJsonDocument::fromJson( file.readAll() ).object();
			file.close();
			if( document.isEmpty() == false )
			{
				summary = tr( "Timetable: %1 - %2 periods - %3 courses - stored locally and on the teacher machine" )
							  .arg( document.value( QStringLiteral("name") ).toString( tr( "(unnamed)" ) ) )
							  .arg( int( document.value( QStringLiteral("periods") ).toArray().size() ) )
							  .arg( int( document.value( QStringLiteral("courses") ).toArray().size() ) );
			}
		}
		m_timetableSummary->setText( summary );
	}
}


void ClassroomMasterDialog::setupTimetableTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );

	auto* timetableCard = new QWidget( page );
	auto* timetableLayout = new QVBoxLayout( timetableCard );
	timetableLayout->addWidget( new QLabel( tr( "Timetable (shared with the phone)" ), timetableCard ) );
	m_timetableSummary = new QLabel( tr( "Timetable not uploaded by the phone yet." ), timetableCard );
	m_timetableSummary->setWordWrap( true );
	timetableLayout->addWidget( m_timetableSummary );

	auto* timetableRow = new QHBoxLayout;
	auto* importTimetable = new QPushButton( tr( "Import timetable JSON" ), timetableCard );
	auto* exportTimetable = new QPushButton( tr( "Export timetable JSON" ), timetableCard );
	timetableRow->addWidget( importTimetable );
	timetableRow->addWidget( exportTimetable );
	timetableRow->addStretch( 1 );
	timetableLayout->addLayout( timetableRow );
	layout->addWidget( timetableCard );

	connect( importTimetable, &QPushButton::clicked, this, [this]() {
		const auto path = QFileDialog::getOpenFileName( this, tr( "Import timetable JSON" ),
													   QString{}, tr( "JSON (*.json);;All files (*)" ) );
		if( path.isEmpty() ) { return; }
		QFile source( path );
		if( source.open( QFile::ReadOnly ) == false ) { return; }
		const auto bytes = source.readAll();
		source.close();
		if( QJsonDocument::fromJson( bytes ).object().isEmpty() ) { return; }
		QDir().mkpath( dataDirectory() );
		QFile target( timetableFilePath() );
		if( target.open( QFile::WriteOnly | QFile::Truncate ) )
		{
			target.write( QJsonDocument( QJsonDocument::fromJson( bytes ).object() ).toJson( QJsonDocument::Compact ) );
			target.close();
		}
		refreshLessonRecords();
	} );
	connect( exportTimetable, &QPushButton::clicked, this, [this]() {
		const auto path = QFileDialog::getSaveFileName( this, tr( "Export timetable JSON" ),
													   QStringLiteral("ikeback-timetable.json"),
													   tr( "JSON (*.json);;All files (*)" ) );
		if( path.isEmpty() ) { return; }
		QFile source( timetableFilePath() );
		if( source.open( QFile::ReadOnly ) == false ) { return; }
		const auto bytes = source.readAll();
		source.close();
		QFile target( path );
		if( target.open( QFile::WriteOnly | QFile::Truncate ) ) { target.write( bytes ); target.close(); }
	} );

	// Per-lesson scoring, bound to the lesson topic the teacher types here.
	auto* lessonCard = new QWidget( page );
	auto* lessonLayout = new QVBoxLayout( lessonCard );
	lessonLayout->addWidget( new QLabel( tr( "This lesson" ), lessonCard ) );
	auto* topicRow = new QHBoxLayout;
	topicRow->addWidget( new QLabel( tr( "Lesson content:" ), lessonCard ) );
	m_lessonTopic = new QLineEdit( lessonCard );
	m_lessonTopic->setPlaceholderText( tr( "What was taught in this lesson" ) );
	topicRow->addWidget( m_lessonTopic, 1 );
	auto* saveTopic = new QPushButton( tr( "Save lesson topic" ), lessonCard );
	topicRow->addWidget( saveTopic );
	lessonLayout->addLayout( topicRow );

	auto* scoreRow = new QHBoxLayout;
	scoreRow->addWidget( new QLabel( tr( "Whole class performance:" ), lessonCard ) );
	const QVector<int> deltas{ 5, 1, -1, -5 };
	for( const auto delta : deltas )
	{
		auto* button = new QPushButton( delta > 0 ? QStringLiteral("+%1").arg( delta ) : QString::number( delta ),
										lessonCard );
		scoreRow->addWidget( button );
		connect( button, &QPushButton::clicked, this, [this, delta]() {
			if( m_scorePanel != nullptr && m_lessonTopic != nullptr )
			{
				const auto topic = m_lessonTopic->text().trimmed();
				ClassroomScoreBook book( dataDirectory() );
				book.load( currentClassName() );
				book.ensureLesson( QDate::currentDate(), topic );
				book.save();
				scoreWholeClass( delta );
				refreshLessonRecords();
			}
		} );
	}
	scoreRow->addStretch( 1 );
	lessonLayout->addLayout( scoreRow );
	connect( saveTopic, &QPushButton::clicked, this, [this]() {
		ClassroomScoreBook book( dataDirectory() );
		book.load( currentClassName() );
		book.ensureLesson( QDate::currentDate(), m_lessonTopic->text().trimmed() );
		book.save();
		appendTeacherEvent( EventType::score(), tr( "Lesson topic: %1" ).arg( m_lessonTopic->text().trimmed() ) );
		refreshLessonRecords();
	} );

	lessonLayout->addWidget( new QLabel( tr( "Class lesson records" ), lessonCard ) );
	m_lessonRecords = new QPlainTextEdit( lessonCard );
	m_lessonRecords->setReadOnly( true );
	m_lessonRecords->setMinimumHeight( 220 );
	lessonLayout->addWidget( m_lessonRecords, 1 );
	auto* refreshRecords = new QPushButton( tr( "Refresh records" ), lessonCard );
	connect( refreshRecords, &QPushButton::clicked, this, &ClassroomMasterDialog::refreshLessonRecords );
	lessonLayout->addWidget( refreshRecords );
	layout->addWidget( lessonCard, 1 );

	m_tabs->addTab( page, tr( "Timetable & lesson" ) );
	refreshLessonRecords();
}


void ClassroomMasterDialog::setupToolsTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );

	auto* scopeRow = new QHBoxLayout;
	m_toolScope = new QComboBox( page );
	m_toolScope->addItem( tr( "All students" ) );
	m_toolScope->addItem( tr( "Selected students (roster table)" ) );
	m_toolsStatus = new QLabel( tr( "Ready." ), page );
	m_toolsStatus->setWordWrap( true );
	scopeRow->addWidget( new QLabel( tr( "Scope:" ), page ) );
	scopeRow->addWidget( m_toolScope );
	scopeRow->addWidget( m_toolsStatus, 1 );
	layout->addLayout( scopeRow );

	auto* note = new QLabel( tr( "Client protection = students cannot quit the ikeback card. "
								 "UWF = Windows write filter / restore on reboot, disabled before installing software. "
								 "They are independent switches." ), page );
	note->setWordWrap( true );
	note->setStyleSheet( QStringLiteral("color:#546e7a; font-size:11px;") );
	layout->addWidget( note );

	// The buttons themselves live on the Ikeback Master tool bar (2.4.0: as
	// first-class buttons, not only inside a drop-down). This page keeps the
	// parameters those actions read and collects the exact same lambdas, so
	// both surfaces share one implementation.
	const auto group = [this]( const QString& label, const QString& icon,
							  const QVector<QuickToolAction>& actions ) {
		QuickToolGroup entry;
		entry.label = label;
		entry.icon = icon;
		entry.actions = actions;
		m_quickToolGroups.append( entry );
	};

	const auto input = [page, layout]( const QString& label, QLineEdit*& field, const QString& placeholder ) {
		auto* row = new QHBoxLayout;
		row->addWidget( new QLabel( label, page ) );
		auto* edit = new QLineEdit( page );
		edit->setPlaceholderText( placeholder );
		row->addWidget( edit, 1 );
		layout->addLayout( row );
		field = edit;
	};

	const auto send = [this]( Command command, bool enabled, const QString& text = QString{} ) {
		auto message = Classroom::message( command );
		message.addArgument( Argument::Enabled, enabled );
		message.addArgument( Argument::Ip,
							 m_teacherIp ? Classroom::sanitizeUnicastIp( m_teacherIp->text() ) : QString{} );
		if( text.isEmpty() == false ) { message.addArgument( Argument::Message, text ); }
		sendToTargets( message );
	};

	const auto feature = [this]( const QString& uid, bool start, bool all ) {
		ComputerControlInterfaceList list;
		if( all ) { list = m_master.allComputerControlInterfaces(); }
		else { list = m_master.selectedComputerControlInterfaces(); }
		IkebackCore::featureManager().controlFeature( Feature::Uid{ uid },
			start ? FeatureProviderInterface::Operation::Start : FeatureProviderInterface::Operation::Stop,
			QVariantMap{}, list );
	};

	// Real demo features. NOTE: 2aca1e9f... (DemoAllScreens) is only an OPTION
	// sub-feature and does NOT start a demo - older builds called controlFeature
	// on it and silently did nothing.
	const auto demoFullUid = QStringLiteral("07b375e1-8ab6-4b48-bcb7-75fb3d56035b");  //!< ShareOwnScreenFullScreen
	const auto demoWindowUid = QStringLiteral("68c55fb9-127e-4c9f-9c90-28b998bf1a47"); //!< ShareOwnScreenWindow
	const auto usbUid = QStringLiteral("e4a77879-e544-4fec-bc18-e534f33b934c");       //!< InputDevicesLock (desktop stays visible)

	const auto runFeature = [this]( const QString& uid, bool start, bool all ) {
		ComputerControlInterfaceList list;
		if( all ) { list = m_master.allComputerControlInterfaces(); }
		else { list = m_master.selectedComputerControlInterfaces(); }
		const auto& demo = IkebackCore::featureManager().feature( Feature::Uid{ uid } );
		if( start ) { IkebackCore::featureManager().startFeature( m_master, demo, list ); }
		else { IkebackCore::featureManager().stopFeature( m_master, demo, list ); }
	};

	// Broadcast: the teacher screen is shown on every student display (never a
	// black screen), USB input is blocked by default. "Watch & do" uses the
	// WINDOW demo so students keep keyboard/mouse and can follow along.
	const auto broadcast = [this, runFeature, feature, demoFullUid, demoWindowUid, usbUid]
		( bool selectedOnly, bool watchAndDo ) {
		const auto all = selectedOnly == false;
		if( watchAndDo )
		{
			runFeature( demoWindowUid, true, all );
			feature( usbUid, false, all );
		}
		else
		{
			runFeature( demoFullUid, true, all );
			feature( usbUid, true, all );
		}
		setToolStatus( watchAndDo ?
			tr( "Watch & do started (window demo, students keep keyboard/mouse, screens stay visible)." ) :
			tr( "Broadcast started (USB input disabled, screens stay visible)." ) );
		appendTeacherEvent( EventType::demo(),
							watchAndDo ? tr( "watch-and-do started" ) : tr( "broadcast started" ) );
	};

	group( tr( "Broadcast" ), QStringLiteral(":/core/media-playback-start.png"), {
		{ QStringLiteral("broadcast.all"), tr( "Broadcast all" ),
		  [broadcast]() { broadcast( false, false ); } },
		{ QStringLiteral("broadcast.selected"), tr( "Broadcast selected" ),
		  [broadcast]() { broadcast( true, false ); } },
		{ QStringLiteral("broadcast.watch"), tr( "Watch & do" ),
		  [broadcast]() { broadcast( false, true ); } },
		{ QStringLiteral("broadcast.stop"), tr( "Stop broadcast" ),
		  [this, runFeature, feature, demoFullUid, demoWindowUid, usbUid]() {
			runFeature( demoFullUid, false, true );
			runFeature( demoWindowUid, false, true );
			feature( usbUid, false, true );
			setToolStatus( tr( "Broadcast stopped, USB input restored." ) );
		} } } );

	group( tr( "Client protection" ), QStringLiteral(":/core/icon22.png"), {
		{ QStringLiteral("protection.on"), tr( "Enable" ), [this]() { setClientProtection( true ); } },
		{ QStringLiteral("protection.off"), tr( "Disable" ), [this]() { setClientProtection( false ); } } } );

	group( tr( "UWF restore" ), QStringLiteral(":/core/document-save.png"), {
		{ QStringLiteral("uwf.on"), tr( "Enable UWF" ), [this]() { setUwfFilter( true ); } },
		{ QStringLiteral("uwf.off"), tr( "Disable UWF" ), [this]() { setUwfFilter( false ); } },
		{ QStringLiteral("uwf.query"), tr( "Query UWF" ), [this]() {
			sendToAll( Classroom::message( Command::UwfQuery ) );
			QTimer::singleShot( 2000, this, [this]() { refreshUwfStatus(); refreshSeatGrid(); } );
			setToolStatus( tr( "UWF query sent; results appear in the seat list view and activity log." ) );
		} },
		{ QStringLiteral("uwf.restart"), tr( "Restart students" ), [this]() {
			sendToAll( Classroom::message( Command::UwfRestart ) );
			setToolStatus( tr( "Reboot command sent to all students." ) );
		} } } );

	group( tr( "Gateway" ), QStringLiteral(":/core/go-next.png"), {
		{ QStringLiteral("gateway.block"), tr( "Disable gateway" ),
		  [send]() { send( Command::SetInternetBlock, true ); } },
		{ QStringLiteral("gateway.allow"), tr( "Enable gateway" ),
		  [send]() { send( Command::SetInternetBlock, false ); } } } );

	input( tr( "Silence page text:" ), m_toolSilence,
		   tr( "Text shown on every student screen, e.g. Please read page 42" ) );
	m_toolSilence->setText( settingsJson().value( SettingKey::silencePageText() ).toString() );
	group( tr( "Silence page" ), QStringLiteral(":/core/document-edit.png"), {
		{ QStringLiteral("silence.show"), tr( "Show" ), [this, send]() {
			auto object = settingsJson();
			object.insert( SettingKey::silencePageText(), m_toolSilence->text() );
			m_settings.setObject( object );
			m_settings.save();
			if( m_silenceText ) { m_silenceText->setText( m_toolSilence->text() ); }
			send( Command::ShowSilencePage, true, m_toolSilence->text() );
			setToolStatus( tr( "Silence page shown and text saved." ) );
		} },
		{ QStringLiteral("silence.hide"), tr( "Hide" ),
		  [send]() { send( Command::HideSilencePage, false ); } } } );

	input( tr( "Blocked apps (fuzzy):" ), m_toolAppRules,
		   tr( "Comma or newline separated, e.g. game, steam, notepad" ) );
	group( tr( "App block" ), QStringLiteral(":/core/edit-delete.png"), {
		{ QStringLiteral("appblock.apply"), tr( "Apply rules" ),
		  [this]() { applyFuzzyRules( SettingKey::blockedApps(), m_toolAppRules->text() ); } },
		{ QStringLiteral("appblock.clear"), tr( "Clear" ), [this]() {
			m_toolAppRules->clear();
			applyFuzzyRules( SettingKey::blockedApps(), {} );
		} } } );

	input( tr( "Blocked sites (fuzzy):" ), m_toolWebRules,
		   tr( "Comma or newline separated, e.g. douyin, bilibili, game" ) );
	group( tr( "Site block" ), QStringLiteral(":/core/edit-delete.png"), {
		{ QStringLiteral("webblock.apply"), tr( "Apply rules" ),
		  [this]() { applyFuzzyRules( SettingKey::blockedWebsites(), m_toolWebRules->text() ); } },
		{ QStringLiteral("webblock.clear"), tr( "Clear" ), [this]() {
			m_toolWebRules->clear();
			applyFuzzyRules( SettingKey::blockedWebsites(), {} );
		} } } );

	input( tr( "Software path:" ), m_toolSoftware, tr( "Full path of the program on the student machine" ) );
	input( tr( "Website URL:" ), m_toolWebsite, tr( "https://..." ) );
	group( tr( "Open on students" ), QStringLiteral(":/core/document-open.png"), {
		{ QStringLiteral("open.software"), tr( "Open software" ),
		  [this]() { openOnTargets( false, m_toolSoftware->text() ); } },
		{ QStringLiteral("open.website"), tr( "Open website" ),
		  [this]() { openOnTargets( true, m_toolWebsite->text() ); } } } );

	group( tr( "Whole class score" ), QStringLiteral(":/core/list-add.png"), {
		{ QStringLiteral("score.p5"), tr( "+5" ), [this]() { scoreWholeClass( 5 ); } },
		{ QStringLiteral("score.p1"), tr( "+1" ), [this]() { scoreWholeClass( 1 ); } },
		{ QStringLiteral("score.m1"), tr( "-1" ), [this]() { scoreWholeClass( -1 ); } },
		{ QStringLiteral("score.m5"), tr( "-5" ), [this]() { scoreWholeClass( -5 ); } },
		{ QStringLiteral("score.custom"), tr( "Custom..." ), [this]() {
			bool accepted = false;
			const auto delta = QInputDialog::getInt( this, windowTitle(), tr( "Points for every student:" ),
													 0, -100, 100, 1, &accepted );
			if( accepted && delta != 0 ) { scoreWholeClass( delta ); }
		} } } );

	// Check-in has its own button on the Ikeback tool bar, so it is deliberately
	// not repeated inside this menu.
	group( tr( "Other" ), QStringLiteral(":/core/icon22.png"), {
		{ QStringLiteral("other.mute"), tr( "Mute all" ), [send]() { send( Command::SetMute, true ); } },
		{ QStringLiteral("other.unmute"), tr( "Unmute all" ), [send]() { send( Command::SetMute, false ); } },
		{ QStringLiteral("other.lock"), tr( "Lock all" ), [this]() { lockAll( true ); } },
		{ QStringLiteral("other.unlock"), tr( "Unlock all" ), [this]() { lockAll( false ); } },
		{ QStringLiteral("other.refresh"), tr( "Refresh" ),
		  [this]() { refreshStatus(); refreshMachines(); } } } );

	auto* toolbarHint = new QLabel(
		tr( "All actions are available from the ikeback tool bar: open the \"Quick tools\" menu, "
			"or press the \"Check-in\" button. This page only holds their parameters." ), page );
	toolbarHint->setWordWrap( true );
	toolbarHint->setStyleSheet( QStringLiteral("color:#546e7a; font-size:11px;") );
	layout->addWidget( toolbarHint );

	layout->addStretch( 1 );
	m_tabs->insertTab( 0, page, tr( "Quick tools" ) );
}


bool ClassroomMasterDialog::runQuickTool( const QString& id )
{
	for( const auto& group : m_quickToolGroups )
	{
		for( const auto& action : group.actions )
		{
			if( action.id == id )
			{
				if( action.run ) { action.run(); }
				return true;
			}
		}
	}
	return false;
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
	// Shield icon = CLIENT protection (cannot quit the student card).
	m_seatGrid->setProtectionProvider( [this]( const QString& ) {
		return settingsJson().value( SettingKey::exitProtection() ).toBool( true );
	} );
	// List view additionally shows the per-machine UWF restore state.
	m_seatGrid->setUwfProvider( [this]( const QString& seat ) {
		return m_uwfStatus.value( seat );
	} );
	m_seatGrid->setIdentityProvider( [this]( const QString& seat ) {
		const auto* classItem = m_roster.findClass( currentClassName() );
		if( classItem == nullptr ) { return QString{}; }
		for( const auto& student : classItem->students )
		{
			if( student.seat == seat ) { return m_status.value( student.ip ); }
		}
		return QString{};
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
	// UWF reports arrive through the activity log; reading it every tick would
	// be wasteful, so refresh at most once every ~30 s (timer interval 5 s).
	if( m_refreshTicks % 6 == 0 ) { refreshUwfStatus(); }
	m_seatGrid->setClassName( currentClassName() );
	m_seatGrid->setOnlineIps( onlineIps() );

	// Publish every student's current score to the matching Ikeback Master
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
				// Qualify through QObject: ComputerControlInterface hides the
				// base setProperty() with a QUuid-keyed overload of its own.
				auto* object = static_cast<QObject*>( controlInterface.data() );
				object->setProperty( "ikebackScore", m_scorePanel->scoreForSeat( student.seat ) );
				object->setProperty( "ikebackProtected",
									 settingsJson().value( SettingKey::exitProtection() ).toBool( true ) );
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
	// The old code silently picked the first non-loopback address, which on a
	// machine with VMware/VirtualBox/Hyper-V adapters is often NOT the LAN
	// address the phone should use. Offer every address explicitly instead.
	m_teacherIpPicker = new QComboBox( page );
	QString preferredIp;
	for( const auto& address : QNetworkInterface::allAddresses() )
	{
		if( address.isLoopback() || address.protocol() != QAbstractSocket::IPv4Protocol ) { continue; }
		const auto ipv4 = address.toIPv4Address();
		const auto first = static_cast<quint8>( ipv4 >> 24 );
		const auto second = static_cast<quint8>( ipv4 >> 16 );
		const auto isPrivate = first == 10 || ( first == 192 && second == 168 ) ||
							   ( first == 172 && second >= 16 && second <= 31 );
		m_teacherIpPicker->addItem( address.toString() );
		if( isPrivate && preferredIp.isEmpty() ) { preferredIp = address.toString(); }
	}
	if( preferredIp.isEmpty() && m_teacherIpPicker->count() > 0 )
	{
		preferredIp = m_teacherIpPicker->itemText( 0 );
	}
	m_teacherIp->setText( preferredIp );
	connect( m_teacherIpPicker, QOverload<int>::of( &QComboBox::activated ), this,
			 [this]( int index ) {
				 m_teacherIp->setText( m_teacherIpPicker->itemText( index ) );
				 syncNow();
			 } );
	m_teacherIp->setPlaceholderText( tr( "Teacher IP kept reachable while Internet is blocked" ) );
	m_teacherIp->setToolTip( tr( "The address the phone must use. Pick the network adapter that is on the "
								 "same Wi-Fi/LAN as the phones." ) );
	m_packagePath = new QLineEdit( page );
	m_packagePath->setPlaceholderText( tr( "Installer path on the teacher machine" ) );
	m_silenceText = new QLineEdit( page );
	m_silenceText->setText( settingsJson().value( SettingKey::silencePageText() ).toString() );
	m_bridgeEnabled = new QCheckBox( tr( "Start mobile bridge" ), page );
	// The bridge is what makes the phone work at all, so it starts by default.
	m_bridgeEnabled->setChecked( settingsJson().value( SettingKey::bridgeEnabled() ).toBool( true ) );
	m_bridgePort = new QSpinBox( page );
	m_bridgePort->setRange( 1024, 65535 );
	m_bridgePort->setValue( settingsJson().value( SettingKey::bridgePort() ).toInt( 11880 ) );
	form->addRow( tr( "Teacher IP:" ), m_teacherIp );
	form->addRow( tr( "Adapter:" ), m_teacherIpPicker );
	form->addRow( tr( "Installer:" ), m_packagePath );
	form->addRow( tr( "Silence page:" ), m_silenceText );
	form->addRow( m_bridgeEnabled );
	form->addRow( tr( "Bridge port:" ), m_bridgePort );
	layout->addLayout( form );

	// ---------------------------------------------------------- connectivity
	auto* checkRow = new QHBoxLayout;
	m_connectivityLabel = new QLabel( page );
	m_connectivityLabel->setWordWrap( true );
	m_connectivityLabel->setText( tr( "Not checked yet." ) );
	auto* checkButton = new QPushButton( tr( "Check connection" ), page );
	auto* restartButton = new QPushButton( tr( "Restart ikeback service" ), page );
	restartButton->setToolTip( tr( "Use this when this computer shows a red icon and students cannot be "
								   "reached. Needs administrator rights." ) );
	checkRow->addWidget( checkButton );
	checkRow->addWidget( restartButton );
	checkRow->addWidget( m_connectivityLabel, 1 );
	layout->addLayout( checkRow );
	connect( checkButton, &QPushButton::clicked, this, &ClassroomMasterDialog::runConnectivityCheck );
	connect( restartButton, &QPushButton::clicked, this, &ClassroomMasterDialog::restartService );

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
		message.addArgument( Argument::Ip, Classroom::sanitizeUnicastIp( m_teacherIp->text() ) );
		if( text.isEmpty() == false ) { message.addArgument( Argument::Message, text ); }
		sendToAll( message );
	};

	const auto feature = [this]( const QString& uid, bool start, const QVariantMap& arguments = QVariantMap{} ) {
		IkebackCore::featureManager().controlFeature( Feature::Uid{ uid },
			start ? FeatureProviderInterface::Operation::Start : FeatureProviderInterface::Operation::Stop,
			arguments, m_master.allComputerControlInterfaces() );
	};

	const auto demo = [this]( const QString& uid, bool start ) {
		const auto& item = IkebackCore::featureManager().feature( Feature::Uid{ uid } );
		if( start ) { IkebackCore::featureManager().startFeature( m_master, item, m_master.allComputerControlInterfaces() ); }
		else { IkebackCore::featureManager().stopFeature( m_master, item, m_master.allComputerControlInterfaces() ); }
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
		{ tr( "Fullscreen broadcast" ), [demo]() { demo( QStringLiteral("07b375e1-8ab6-4b48-bcb7-75fb3d56035b"), true ); } },
		{ tr( "Window demo" ), [demo]() { demo( QStringLiteral("68c55fb9-127e-4c9f-9c90-28b998bf1a47"), true ); } },
		{ tr( "Stop demo" ), [demo]() {
			demo( QStringLiteral("07b375e1-8ab6-4b48-bcb7-75fb3d56035b"), false );
			demo( QStringLiteral("68c55fb9-127e-4c9f-9c90-28b998bf1a47"), false );
		} } } );
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
			if( command.isEmpty() )
			{
				QMessageBox::warning( this, windowTitle(),
									  tr( "Only .exe and .msi installers can be deployed, and the path must not contain quotes." ) );
				return;
			}
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
		if( m_bridge->start( m_bridgePort->value(), QHostAddress( m_teacherIp->text().trimmed() ) ) == false )
		{
			qWarning( "ikeback: mobile bridge did not start" );
		}
		m_systemStatus->setText( m_bridge->status() );
	}
	// Show the local server/bridge state right away: the red "this computer"
	// icon in the Ikeback window finally gets an explanation.
	QTimer::singleShot( 400, this, &ClassroomMasterDialog::runConnectivityCheck );
	m_tabs->addTab( page, tr( "System & devices" ) );
}
