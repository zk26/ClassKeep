/*
 * ClassroomBridge.cpp - lightweight teacher machine HTTP/JSON bridge for the mobile app
 *
 * This file is part of the Veyon classroom modification.
 */

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QUuid>

#include "ClassroomBridge.h"
#include "ClassroomData.h"
#include "ClassroomProtocol.h"
#include "ClassroomScore.h"
#include "ClassroomSeatOrder.h"
#include "ClassroomSecrets.h"
#include "ClassroomTermReport.h"
#include "ClassroomWordCloud.h"
#include "ComputerControlInterface.h"
#include "FeatureManager.h"
#include "FeatureProviderInterface.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"


static QByteArray jsonResponse( const QJsonObject& object )
{
	return QJsonDocument( object ).toJson( QJsonDocument::Compact );
}


ClassroomBridge::ClassroomBridge( const QString& dataDirectory, ClassroomRoster& roster,
								  ClassroomLogStore& store, const QJsonObject& settings,
								  VeyonMasterInterface& master, QObject* parent ) :
	QObject( parent ),
	m_dataDirectory( dataDirectory ),
	m_roster( roster ),
	m_store( store ),
	m_master( master ),
	m_settings( settings ),
	m_book( dataDirectory ),
	m_teacherId( QUuid::createUuid().toString( QUuid::WithoutBraces ).left( 12 ) ),
	m_sessionKey( ClassroomSecrets::reveal( settings.value( QStringLiteral("sessionKey") ).toString() ) )
{
	rotateNonce();
	m_server = new QTcpServer( this );
	connect( m_server, &QTcpServer::newConnection, this, &ClassroomBridge::acceptConnection );

	m_udp = new QUdpSocket( this );
	connect( m_udp, &QUdpSocket::readyRead, this, &ClassroomBridge::readDiscovery );

	m_broadcastTimer = new QTimer( this );
	m_broadcastTimer->setInterval( 5000 );
	connect( m_broadcastTimer, &QTimer::timeout, this, &ClassroomBridge::broadcast );
}


bool ClassroomBridge::start( int port )
{
	if( m_server->isListening() ) { return true; }
	if( m_server->listen( QHostAddress::Any, static_cast<quint16>( port ) ) == false ) { return false; }
	m_port = port;
	m_udp->bind( static_cast<quint16>( port ), QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint );
	m_broadcastTimer->start();
	broadcast();
	return true;
}


void ClassroomBridge::stop()
{
	m_broadcastTimer->stop();
	m_udp->close();
	m_server->close();
}


bool ClassroomBridge::isRunning() const
{
	return m_server->isListening();
}


QString ClassroomBridge::status() const
{
	if( isRunning() == false ) { return tr( "Bridge stopped" ); }
	return tr( "Bridge on port %1 (revision %2, lesson key %3)" )
			.arg( m_port ).arg( revision() )
			.arg( m_sessionKey.isEmpty() ? tr( "missing - start a new lesson" ) : tr( "active" ) );
}


void ClassroomBridge::syncNow()
{
	broadcast();
}


void ClassroomBridge::setSettings( const QJsonObject& settings )
{
	m_settings = settings;
	const auto key = ClassroomSecrets::reveal( settings.value( QStringLiteral("sessionKey") ).toString() );
	if( key != m_sessionKey )
	{
		// A new lesson key invalidates every previously issued token immediately.
		m_sessionKey = key;
		rotateNonce();
	}
}


void ClassroomBridge::rotateNonce()
{
	m_previousNonce = m_nonce;
	m_nonce = QUuid::createUuid().toString( QUuid::WithoutBraces ).left( 16 );
	m_nonceIssued = QDateTime::currentDateTime();
}


QString ClassroomBridge::tokenFor( const QString& nonce ) const
{
	return ClassroomSecrets::sessionToken( m_sessionKey, nonce );
}


static void noteAuthFailure( int* count, QDateTime* lockUntil )
{
	if( ++*count < 10 ) { return; }
	*count = 0;
	*lockUntil = QDateTime::currentDateTime().addSecs( 60 );
}


bool ClassroomBridge::authorized( const QStringList& headerLines ) const
{
	// Fail closed: the teacher must have started a lesson and generated a key.
	if( m_sessionKey.isEmpty() || m_nonce.isEmpty() ) { return false; }
	if( m_authLockUntil.isValid() && QDateTime::currentDateTime() < m_authLockUntil ) { return false; }

	QString nonce;
	QString token;
	for( const auto& line : headerLines )
	{
		const auto colon = line.indexOf( QLatin1Char(':') );
		if( colon < 0 ) { continue; }
		const auto name = line.left( colon ).trimmed().toLower();
		const auto value = line.mid( colon + 1 ).trimmed();
		if( name == QLatin1String("x-classroom-nonce") ) { nonce = value; }
		else if( name == QLatin1String("x-classroom-token") ) { token = value; }
	}
	if( nonce.isEmpty() || token.isEmpty() ||
		( nonce != m_nonce && nonce != m_previousNonce ) )
	{
		noteAuthFailure( &m_failedAuths, &m_authLockUntil );
		return false;
	}

	const auto expected = tokenFor( nonce );
	if( expected.size() != token.size() )
	{
		noteAuthFailure( &m_failedAuths, &m_authLockUntil );
		return false;
	}
	unsigned char difference = 0;
	for( int index = 0; index < expected.size(); ++index )
	{
		const auto left = static_cast<unsigned char>( expected.at( index ).unicode() );
		const auto right = static_cast<unsigned char>( token.at( index ).unicode() );
		difference = static_cast<unsigned char>( difference ^ ( left ^ right ) );
	}
	if( difference != 0 )
	{
		noteAuthFailure( &m_failedAuths, &m_authLockUntil );
		return false;
	}
	m_failedAuths = 0;
	return true;
}


qint64 ClassroomBridge::revision() const
{
	const QFileInfo scores( QDir( m_dataDirectory ).filePath( QStringLiteral("scores") ) );
	const QFileInfo settings( QDir( m_dataDirectory ).filePath( QStringLiteral("settings.json") ) );
	return qMax( scores.lastModified().toMSecsSinceEpoch(), settings.lastModified().toMSecsSinceEpoch() );
}


QJsonObject ClassroomBridge::stateObject() const
{
	return { { QStringLiteral("teacherId"), m_teacherId },
			 { QStringLiteral("revision"), revision() },
			 { QStringLiteral("port"), m_port },
			 { QStringLiteral("className"), m_className },
			 { QStringLiteral("syncEnabled"), m_settings.value( Classroom::SettingKey::syncEnabled() ).toBool( true ) },
			 { QStringLiteral("nonce"), m_nonce },
			 { QStringLiteral("keyConfigured"), m_sessionKey.isEmpty() == false },
			 { QStringLiteral("classNames"), QJsonArray::fromStringList( m_roster.classNames() ) } };
}


QString ClassroomBridge::timetableFilePath() const
{
	return QDir( m_dataDirectory ).filePath( QStringLiteral("timetable.json") );
}


QByteArray ClassroomBridge::announcementPayload()
{
	// Only a challenge value is broadcast - the lesson key never leaves the teacher machine.
	return QStringLiteral("%1%2:%3:%4:%5")
			.arg( Classroom::announcementPrefix(), QString::number( m_port ), m_teacherId,
				  QString::number( revision() ), m_nonce ).toUtf8();
}


void ClassroomBridge::broadcast()
{
	// Rotate even when the UDP socket is unavailable, otherwise a stale nonce
	// would keep weakening replay protection forever.
	if( m_nonceIssued.secsTo( QDateTime::currentDateTime() ) >= 60 ) { rotateNonce(); }
	if( m_udp->state() != QUdpSocket::BoundState ) { return; }
	m_udp->writeDatagram( announcementPayload(), QHostAddress::Broadcast, static_cast<quint16>( m_port ) );
}


void ClassroomBridge::readDiscovery()
{
	while( m_udp->hasPendingDatagrams() )
	{
		QByteArray datagram;
		datagram.resize( static_cast<int>( m_udp->pendingDatagramSize() ) );
		QHostAddress sender;
		quint16 senderPort = 0;
		m_udp->readDatagram( datagram.data(), datagram.size(), &sender, &senderPort );
		if( datagram.startsWith( Classroom::discoveryMagic().toUtf8() ) )
		{
			// Reply with the same announcement format the app parses, so an
			// active probe works even if the periodic broadcast was missed.
			m_udp->writeDatagram( announcementPayload(), sender, senderPort );
		}
	}
}


void ClassroomBridge::acceptConnection()
{
	while( m_server->hasPendingConnections() )
	{
		auto* socket = m_server->nextPendingConnection();
		connect( socket, &QTcpSocket::readyRead, this, &ClassroomBridge::readRequest );
		connect( socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater );
	}
}


void ClassroomBridge::respond( QTcpSocket* socket, const QByteArray& body, const QByteArray& contentType )
{
	QByteArray header;
	header += QByteArrayLiteral("HTTP/1.1 200 OK\r\n");
	header += QByteArrayLiteral("Access-Control-Allow-Origin: *\r\n");
	header += QByteArrayLiteral("Access-Control-Allow-Headers: Content-Type, X-Classroom-Nonce, X-Classroom-Token\r\n");
	header += QByteArrayLiteral("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
	header += QByteArrayLiteral("Content-Type: ") + contentType + QByteArrayLiteral("\r\n");
	header += QByteArrayLiteral("Content-Length: ") + QByteArray::number( body.size() ) + QByteArrayLiteral("\r\n");
	header += QByteArrayLiteral("Connection: close\r\n\r\n");
	socket->write( header );
	socket->write( body );
	socket->disconnectFromHost();
}


void ClassroomBridge::respondDenied( QTcpSocket* socket )
{
	const auto body = jsonResponse( QJsonObject{
		{ QStringLiteral("error"), QStringLiteral("unauthorized") },
		{ QStringLiteral("hint"), QStringLiteral("Enter the current lesson key in the app") } } );
	QByteArray header;
	header += QByteArrayLiteral("HTTP/1.1 401 Unauthorized\r\n");
	header += QByteArrayLiteral("Access-Control-Allow-Origin: *\r\n");
	header += QByteArrayLiteral("Access-Control-Allow-Headers: Content-Type, X-Classroom-Nonce, X-Classroom-Token\r\n");
	header += QByteArrayLiteral("Content-Type: application/json; charset=utf-8\r\n");
	header += QByteArrayLiteral("Content-Length: ") + QByteArray::number( body.size() ) + QByteArrayLiteral("\r\n");
	header += QByteArrayLiteral("Connection: close\r\n\r\n");
	socket->write( header );
	socket->write( body );
	socket->disconnectFromHost();
}


void ClassroomBridge::readRequest()
{
	auto* socket = qobject_cast<QTcpSocket*>( sender() );
	if( socket == nullptr ) { return; }
	const auto raw = socket->readAll();
	const auto separator = raw.indexOf( QByteArrayLiteral("\r\n\r\n") );
	if( separator < 0 ) { return; }

	const auto head = QString::fromUtf8( raw.left( separator ) );
	const auto body = raw.mid( separator + 4 );
	const auto lines = head.split( QLatin1Char('\n') );
	if( lines.isEmpty() ) { return; }
	const auto parts = lines.first().trimmed().split( QLatin1Char(' ') );
	if( parts.size() < 2 ) { return; }

	const auto method = parts.at( 0 );
	const auto pathAndQuery = parts.at( 1 );
	if( method == QStringLiteral("OPTIONS") )
	{
		respond( socket, QByteArray() );
		return;
	}

	// Discovery/heartbeat is public metadata only; every data and control route
	// requires the lesson key bound to this teacher machine.
	const auto path = pathAndQuery.section( QLatin1Char('?'), 0, 0 );
	if( path != QStringLiteral("/api/classroom/state") && authorized( lines ) == false )
	{
		respondDenied( socket );
		return;
	}
	handleRoute( socket, method, pathAndQuery, body );
}


void ClassroomBridge::handleCommand( const QJsonObject& command, bool* handled )
{
	*handled = false;
	const auto name = command.value( QStringLiteral("name") ).toString();
	auto enabled = command.value( QStringLiteral("enabled") ).toBool( true );
	const auto text = command.value( QStringLiteral("text") ).toString();

	Classroom::Command messageCommand = Classroom::Command::Ping;
	QString featureUid;
	if( name == QStringLiteral("mute") ) { messageCommand = Classroom::Command::SetMute; }
	else if( name == QStringLiteral("internet") ) { messageCommand = Classroom::Command::SetInternetBlock; }
	else if( name == QStringLiteral("silence") )
	{
		messageCommand = enabled ? Classroom::Command::ShowSilencePage : Classroom::Command::HideSilencePage;
	}
	else if( name == QStringLiteral("uwfDisable") ) { messageCommand = Classroom::Command::UwfSetFilter; enabled = false; }
	else if( name == QStringLiteral("uwfEnable") ) { messageCommand = Classroom::Command::UwfSetFilter; enabled = true; }
	else if( name == QStringLiteral("uwfQuery") ) { messageCommand = Classroom::Command::UwfQuery; }
	else if( name == QStringLiteral("scoreRules") ) { messageCommand = Classroom::Command::SetScoreRules; }
	else if( name == QStringLiteral("usb") )
	{
		featureUid = QStringLiteral("e4a77879-e544-4fec-bc18-e534f33b934c");
	}
	else if( name == QStringLiteral("remote") )
	{
		featureUid = QStringLiteral("ca00ad68-1709-4abe-85e2-48dff6ccf8a2");
		enabled = true;
	}
	else if( name == QStringLiteral("demo") )
	{
		featureUid = QStringLiteral("7b6231bd-eb89-45d3-af32-f70663b2f878");
		enabled = true;
	}
	else { return; }

	if( featureUid.isEmpty() == false )
	{
		VeyonCore::featureManager().controlFeature( Feature::Uid{ featureUid },
			enabled ? FeatureProviderInterface::Operation::Start : FeatureProviderInterface::Operation::Stop,
			{}, m_master.allComputerControlInterfaces() );
	}
	else
	{
		auto message = Classroom::message( messageCommand );
		message.addArgument( Classroom::Argument::Enabled, enabled )
			   .addArgument( Classroom::Argument::Ip,
							 command.value( QStringLiteral("teacherIp") ).toString() )
			   .addArgument( Classroom::Argument::Message, text );
		for( const auto& controlInterface : m_master.allComputerControlInterfaces() )
		{
			controlInterface->sendFeatureMessage( message );
		}
	}
	*handled = true;
}


void ClassroomBridge::handleRoute( QTcpSocket* socket, const QString& method,
								   const QString& pathAndQuery, const QByteArray& body )
{
	const auto path = pathAndQuery.section( QLatin1Char('?'), 0, 0 );
	if( path == QStringLiteral("/api/classroom/state") )
	{
		respond( socket, jsonResponse( stateObject() ) );
		return;
	}
	if( path == QStringLiteral("/api/classroom/roster") )
	{
		respond( socket, m_roster.toJson().toUtf8() );
		return;
	}
	if( path == QStringLiteral("/api/classroom/scores") )
	{
		m_book.load( m_className );
		respond( socket, jsonResponse( m_book.syncSnapshot() ) );
		return;
	}
	// The mobile app renders student cards in exactly the order the teacher
	// dragged them on the seat grid. The order is persisted by ClassroomSeatGrid.
	if( path == QStringLiteral("/api/classroom/seats") )
	{
		m_book.load( m_className );
		const auto* classItem = m_roster.findClass( m_className );
		QVector<ClassroomStudent> ordered;
		if( classItem != nullptr )
		{
			for( const auto& seat : Classroom::loadSeatOrder( m_className ) )
			{
				for( const auto& student : classItem->students )
				{
					if( student.seat == seat ) { ordered.append( student ); break; }
				}
			}
			for( const auto& student : classItem->students )
			{
				bool placed = false;
				for( const auto& item : ordered )
				{
					if( item.seat == student.seat ) { placed = true; break; }
				}
				if( placed == false ) { ordered.append( student ); }
			}
		}
		QJsonArray seats;
		for( const auto& student : ordered )
		{
			seats.append( QJsonObject{
				{ QStringLiteral("seat"), student.seat },
				{ QStringLiteral("student"), student.name },
				{ QStringLiteral("ip"), student.ip },
				{ QStringLiteral("score"), m_book.total( student.seat ) } } );
		}
		respond( socket, jsonResponse( QJsonObject{
			{ QStringLiteral("className"), m_className },
			{ QStringLiteral("seats"), seats } } ) );
		return;
	}
	// One-tap scoring from the phone. Uses the same score book as the teacher UI.
	if( path == QStringLiteral("/api/classroom/score") && method == QStringLiteral("POST") )
	{
		const auto request = QJsonDocument::fromJson( body ).object();
		const auto seat = request.value( QStringLiteral("seat") ).toString();
		const auto delta = request.value( QStringLiteral("delta") ).toInt();
		const auto reason = request.value( QStringLiteral("reason") ).toString();
		bool ok = false;
		if( seat.isEmpty() == false && delta != 0 && m_className.isEmpty() == false )
		{
			m_book.load( m_className );
			const auto lessonId = m_book.ensureLesson( QDate::currentDate(), tr( "Mobile scoring" ) );
			QString studentName;
			if( const auto* classItem = m_roster.findClass( m_className ) )
			{
				for( const auto& student : classItem->students )
				{
					if( student.seat == seat ) { studentName = student.name; break; }
				}
			}
			ClassroomScoreEntry entry;
			entry.lessonId = lessonId;
			entry.className = m_className;
			entry.seat = seat;
			entry.student = studentName;
			entry.delta = delta;
			entry.label = reason.isEmpty() ? QString::number( delta ) : reason;
			entry.author = QStringLiteral("Mobile");
			m_book.addEntry( entry );
			ok = true;
		}
		respond( socket, jsonResponse( QJsonObject{ { QStringLiteral("ok"), ok } } ) );
		return;
	}
	if( path == QStringLiteral("/api/classroom/report") )
	{
		m_book.load( m_className );
		ClassroomTermReport::Options options;
		options.className = m_className;
		options.attendanceColumn = true;
		respond( socket, ClassroomTermReport::toSpreadsheetMl( m_roster, m_book, options ),
				 QByteArrayLiteral("application/vnd.ms-excel; charset=utf-8") );
		return;
	}
	if( path == QStringLiteral("/api/classroom/wordcloud") )
	{
		const auto seat = pathAndQuery.section( QStringLiteral("seat="), 1 ).section( QLatin1Char('&'), 0, 0 );
		QStringList lines;
		for( const auto& event : m_store.load( QDate::currentDate().addDays( -180 ), QDate::currentDate() ) )
		{
			if( event.seat == seat && event.className == m_className )
			{
				lines.append( QStringLiteral("%1 %2").arg( event.detail, event.app ) );
			}
		}
		const auto words = ClassroomWordCloud::tokenize( lines, {}, 60 );
		respond( socket, ClassroomWordCloud::toSvg( words ), QByteArrayLiteral("image/svg+xml; charset=utf-8") );
		return;
	}
	if( path == QStringLiteral("/api/classroom/events") )
	{
		QJsonArray array;
		const auto events = m_store.load( QDate::currentDate().addDays( -30 ), QDate::currentDate() );
		const int limit = 2000;
		const int count = static_cast<int>( events.size() );
		for( int index = qMax( 0, count - limit ); index < count; ++index )
		{
			array.append( events.at( index ).toJson() );
		}
		QJsonObject result{ { QStringLiteral("events"), array } };
		respond( socket, jsonResponse( result ) );
		return;
	}
	// ------------------------------------------------------------------ timetable
	// 2.0.x: the phone owns the timetable but keeps a copy on the teacher machine
	// so a replacement phone can pull it back. The JSON is the ClassBoard
	// single-document format and is stored verbatim next to the classroom data.
	if( path == QStringLiteral("/api/classroom/timetable") && method == QStringLiteral("GET") )
	{
		QFile file( timetableFilePath() );
		QJsonObject document;
		if( file.open( QFile::ReadOnly ) )
		{
			document = QJsonDocument::fromJson( file.readAll() ).object();
			file.close();
		}
		respond( socket, jsonResponse( QJsonObject{
			{ QStringLiteral("timetable"), document },
			{ QStringLiteral("revision"), document.value( QStringLiteral("revision") ).toInt( 0 ) } } ) );
		return;
	}
	if( path == QStringLiteral("/api/classroom/timetable") && method == QStringLiteral("POST") )
	{
		const auto request = QJsonDocument::fromJson( body ).object();
		const auto document = request.value( QStringLiteral("timetable") ).toObject();
		bool ok = false;
		if( document.isEmpty() == false )
		{
			QDir().mkpath( m_dataDirectory );
			QFile file( timetableFilePath() );
			if( file.open( QFile::WriteOnly | QFile::Truncate ) )
			{
				ok = file.write( QJsonDocument( document ).toJson( QJsonDocument::Compact ) ) >= 0;
				file.close();
			}
		}
		respond( socket, jsonResponse( QJsonObject{
			{ QStringLiteral("ok"), ok },
			{ QStringLiteral("revision"), revision() } } ) );
		return;
	}
	// 「班级」 page: lessons / entries / events grouped by class, so the phone
	// can show a natural per-class record instead of a one-off "query lesson".
	if( path == QStringLiteral("/api/classroom/classes") )
	{
		QJsonArray classes;
		const auto* classItem = m_roster.findClass( m_className );
		if( classItem != nullptr )
		{
			m_book.load( m_className );
			QJsonArray students;
			for( const auto& student : classItem->students )
			{
				students.append( QJsonObject{
					{ QStringLiteral("seat"), student.seat },
					{ QStringLiteral("student"), student.name },
					{ QStringLiteral("ip"), student.ip },
					{ QStringLiteral("score"), m_book.total( student.seat ) } } );
			}
			classes.append( QJsonObject{
				{ QStringLiteral("name"), m_className },
				{ QStringLiteral("students"), students } } );
		}
		respond( socket, jsonResponse( QJsonObject{ { QStringLiteral("classes"), classes } } ) );
		return;
	}
	if( path == QStringLiteral("/api/classroom/command") && method == QStringLiteral("POST") )
	{
		bool handled = false;
		handleCommand( QJsonDocument::fromJson( body ).object(), &handled );
		respond( socket, jsonResponse( QJsonObject{ { QStringLiteral("ok"), handled } } ) );
		return;
	}
	if( path == QStringLiteral("/api/classroom/sync") && method == QStringLiteral("POST") )
	{
		const auto document = QJsonDocument::fromJson( body );
		const auto snapshot = document.object().value( QStringLiteral("scores") ).toObject();
		if( snapshot.isEmpty() == false && m_book.applySnapshot( snapshot ) )
		{
			Q_EMIT remoteDataApplied();
			respond( socket, jsonResponse( QJsonObject{ { QStringLiteral("ok"), true },
													  { QStringLiteral("revision"), revision() } } ) );
		}
		else
		{
			respond( socket, jsonResponse( QJsonObject{ { QStringLiteral("ok"), false } } ) );
		}
		return;
	}

	respond( socket, jsonResponse( QJsonObject{ { QStringLiteral("error"), QStringLiteral("unknown route") } } ) );
}
