/*
 * ClassroomBridge.cpp - lightweight teacher machine HTTP/JSON bridge for the mobile app
 *
 * This file is part of the Veyon classroom modification.
 */

#include <QAbstractSocket>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QFutureWatcher>
#include <QPointer>
#include <QTcpServer>
#include <QtConcurrent>
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


QHostAddress ClassroomBridge::localUnicastAddress()
{
	// Prefer a private RFC1918 IPv4 address; never return a wildcard,
	// loopback, multicast or broadcast address.
	const auto addresses = QNetworkInterface::allAddresses();
	for( const auto& address : addresses )
	{
		if( Classroom::unicastAddress( address.toString() ).isNull() ) { continue; }
		const auto ipv4 = address.toIPv4Address();
		const auto first = static_cast<quint8>( ipv4 >> 24 );
		const auto second = static_cast<quint8>( ipv4 >> 16 );
		if( first == 10 || ( first == 192 && second == 168 ) ||
			( first == 172 && second >= 16 && second <= 31 ) )
		{
			return address;
		}
	}
	for( const auto& address : addresses )
	{
		if( Classroom::unicastAddress( address.toString() ).isNull() == false ) { return address; }
	}
	return {};
}


bool ClassroomBridge::start( int port, const QHostAddress& bindAddress )
{
	if( m_server->isListening() ) { return true; }
	auto address = bindAddress;
	if( address.isNull() ) { address = localUnicastAddress(); }
	if( ClassroomBridge::sanitizeTeacherIp( address.toString() ).isEmpty() )
	{
		qWarning( "ikeback: refusing to start the bridge on %s (not a LAN unicast address)",
				  qPrintable( address.toString() ) );
		return false;
	}
	if( m_server->listen( address, static_cast<quint16>( port ) ) == false )
	{
		qWarning( "ikeback: bridge could not listen on %s:%d: %s", qPrintable( address.toString() ),
				  port, qPrintable( m_server->errorString() ) );
		return false;
	}
	m_bindAddress = address;
	m_port = port;
	// Discovery is a public, metadata-only UDP beacon. Binding it to one NIC
	// prevents broadcast replies on multi-homed machines, so only the TCP/HTTP
	// surface is restricted to the validated LAN address.
	if( m_udp->bind( QHostAddress::Any, static_cast<quint16>( port ),
					 QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint ) == false )
	{
		qWarning( "ikeback: discovery socket could not bind: %s", qPrintable( m_udp->errorString() ) );
	}
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
	return tr( "Bridge on %1:%2 (revision %3, lesson key %4)" )
			.arg( m_bindAddress.toString(), QString::number( m_port ), QString::number( revision() ),
				  m_sessionKey.isEmpty() ? tr( "missing - start a new lesson" ) : tr( "active" ) );
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
	// This value is only an announcement/beacon challenge; authentication uses
	// the one-time values handed out by /state.
	m_nonce = ClassroomSecrets::generateNonce();
	m_nonceIssued = QDateTime::currentDateTime();
}


QString ClassroomBridge::issueNonce()
{
	purgeStaleNonces();
	const auto nonce = ClassroomSecrets::generateNonce();
	m_issuedNonces.insert( nonce, QDateTime::currentDateTime() );
	return nonce;
}


void ClassroomBridge::purgeStaleNonces()
{
	const auto now = QDateTime::currentDateTime();
	for( auto it = m_issuedNonces.begin(); it != m_issuedNonces.end(); )
	{
		if( it.value().addSecs( NonceLifetimeSeconds ) < now ) { it = m_issuedNonces.erase( it ); }
		else { ++it; }
	}
	// Bound memory even if a peer floods /state without ever authenticating.
	while( m_issuedNonces.size() > 4096 )
	{
		auto oldest = m_issuedNonces.constBegin();
		for( auto it = m_issuedNonces.constBegin(); it != m_issuedNonces.constEnd(); ++it )
		{
			if( it.value() < oldest.value() ) { oldest = it; }
		}
		m_issuedNonces.remove( oldest.key() );
	}
}


void ClassroomBridge::noteAuthFailure( const QString& peer )
{
	auto& failure = m_authFailures[peer];
	if( ++failure.count < AuthFailureLimit ) { return; }
	failure.count = 0;
	failure.lockUntil = QDateTime::currentDateTime().addSecs( AuthLockSeconds );
	qWarning( "ikeback: bridge authentication locked for %s after %d failures",
			  qPrintable( peer ), AuthFailureLimit );
}


bool ClassroomBridge::authorized( const QHostAddress& peer, const QStringList& headerLines,
								  const QString& method, const QString& pathAndQuery, const QByteArray& body )
{
	// Fail closed: the teacher must have started a lesson and generated a key.
	const auto peerKey = peer.toString();
	const auto now = QDateTime::currentDateTime();
	if( m_sessionKey.isEmpty() ) { return false; }
	const auto failure = m_authFailures.value( peerKey );
	if( failure.lockUntil.isValid() && now < failure.lockUntil ) { return false; }

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

	// Consume the nonce BEFORE any other check: a replay of a valid request
	// (or of the very same nonce with a different body) must never succeed.
	purgeStaleNonces();
	const auto issued = m_issuedNonces.take( nonce );
	if( nonce.isEmpty() || token.isEmpty() || issued.isValid() == false ||
		issued.addSecs( NonceLifetimeSeconds ) < now )
	{
		noteAuthFailure( peerKey );
		return false;
	}

	const auto expected = ClassroomSecrets::requestToken( m_sessionKey, method, pathAndQuery, nonce, body );
	if( ClassroomSecrets::constantTimeEquals( expected, token.toUtf8() ) == false )
	{
		noteAuthFailure( peerKey );
		return false;
	}
	m_authFailures.remove( peerKey );
	return true;
}


QString ClassroomBridge::sanitizeTeacherIp( const QString& value )
{
	// netsh accepts CIDR ranges such as 0.0.0.0/0 as remoteip=. The shared
	// validator rejects anything that is not a plain unicast IPv4 address.
	return Classroom::sanitizeUnicastIp( value );
}


qint64 ClassroomBridge::revision() const
{
	const QFileInfo scores( QDir( m_dataDirectory ).filePath( QStringLiteral("scores") ) );
	const QFileInfo settings( QDir( m_dataDirectory ).filePath( QStringLiteral("settings.json") ) );
	return qMax( scores.lastModified().toMSecsSinceEpoch(), settings.lastModified().toMSecsSinceEpoch() );
}


QJsonObject ClassroomBridge::stateObject()
{
	return { { QStringLiteral("teacherId"), m_teacherId },
			 { QStringLiteral("revision"), revision() },
			 { QStringLiteral("port"), m_port },
			 { QStringLiteral("bindAddress"), m_bindAddress.toString() },
			 { QStringLiteral("className"), m_className },
			 { QStringLiteral("syncEnabled"), m_settings.value( Classroom::SettingKey::syncEnabled() ).toBool( true ) },
			 // A fresh one-time challenge for exactly one authenticated request.
			 { QStringLiteral("nonce"), issueNonce() },
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
		m_buffers.insert( socket, QByteArray() );
		connect( socket, &QTcpSocket::readyRead, this, &ClassroomBridge::readRequest );
		connect( socket, &QTcpSocket::disconnected, this, [this, socket]() {
			m_buffers.remove( socket );
			socket->deleteLater();
		} );
	}
}


ClassroomBridge::Request ClassroomBridge::parseRequest( const QByteArray& buffer )
{
	Request request;
	const auto separator = buffer.indexOf( QByteArrayLiteral("\r\n\r\n") );
	if( separator < 0 )
	{
		request.tooLarge = buffer.size() > MaxHeaderBytes;
		return request;
	}
	if( separator > MaxHeaderBytes ) { request.tooLarge = true; return request; }

	const auto lines = QString::fromUtf8( buffer.left( separator ) ).split( QLatin1Char('\n') );
	if( lines.isEmpty() ) { request.tooLarge = true; return request; }
	const auto parts = lines.first().trimmed().split( QLatin1Char(' ') );
	if( parts.size() < 2 ) { request.tooLarge = true; return request; }
	request.method = parts.at( 0 );
	request.pathAndQuery = parts.at( 1 );
	request.headerLines = lines;

	qint64 contentLength = 0;
	bool chunked = false;
	for( int index = 1; index < lines.size(); ++index )
	{
		const auto line = lines.at( index );
		const auto colon = line.indexOf( QLatin1Char(':') );
		if( colon < 0 ) { continue; }
		const auto name = line.left( colon ).trimmed().toLower();
		const auto value = line.mid( colon + 1 ).trimmed();
		if( name == QLatin1String("content-length") )
		{
			bool ok = false;
			contentLength = value.toLongLong( &ok );
			if( ok == false || contentLength < 0 ) { request.tooLarge = true; return request; }
		}
		else if( name == QLatin1String("transfer-encoding") &&
				 value.contains( QStringLiteral("chunked"), Qt::CaseInsensitive ) )
		{
			chunked = true;
		}
	}
	// No chunked encoding support, and no body that could exhaust the GUI
	// thread's memory before authentication has even been attempted.
	if( chunked || contentLength > MaxBodyBytes ) { request.tooLarge = true; return request; }

	const auto bodyStart = separator + 4;
	if( buffer.size() - bodyStart < contentLength ) { return request; }
	request.body = buffer.mid( bodyStart, static_cast<int>( contentLength ) );
	request.complete = true;
	return request;
}


void ClassroomBridge::writeResponse( QTcpSocket* socket, int statusCode, const QByteArray& reason,
									 const QByteArray& body, const QByteArray& contentType )
{
	QByteArray header;
	header += QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number( statusCode ) + QByteArrayLiteral(" ") +
			  reason + QByteArrayLiteral("\r\n");
	if( contentType.isEmpty() == false )
	{
		header += QByteArrayLiteral("Content-Type: ") + contentType + QByteArrayLiteral("\r\n");
	}
	header += QByteArrayLiteral("Content-Length: ") + QByteArray::number( body.size() ) + QByteArrayLiteral("\r\n");
	header += QByteArrayLiteral("Cache-Control: no-store\r\n");
	header += QByteArrayLiteral("Connection: close\r\n\r\n");
	socket->write( header );
	socket->write( body );
	socket->disconnectFromHost();
}


void ClassroomBridge::respond( QTcpSocket* socket, const QByteArray& body, const QByteArray& contentType )
{
	writeResponse( socket, 200, QByteArrayLiteral("OK"), body, contentType );
}


void ClassroomBridge::respondDenied( QTcpSocket* socket )
{
	respondError( socket, 401, QByteArrayLiteral("Unauthorized"), QStringLiteral("unauthorized") );
}


void ClassroomBridge::respondError( QTcpSocket* socket, int statusCode, const QByteArray& reason,
								   const QString& error )
{
	const auto body = jsonResponse( QJsonObject{ { QStringLiteral("error"), error } } );
	writeResponse( socket, statusCode, reason, body, QByteArrayLiteral("application/json; charset=utf-8") );
}


void ClassroomBridge::readRequest()
{
	auto* socket = qobject_cast<QTcpSocket*>( sender() );
	if( socket == nullptr ) { return; }

	auto& buffer = m_buffers[socket];
	if( buffer.size() + socket->bytesAvailable() > MaxHeaderBytes + MaxBodyBytes + 4096 )
	{
		respondError( socket, 413, QByteArrayLiteral("Payload Too Large"), QStringLiteral("request too large") );
		m_buffers.remove( socket );
		return;
	}
	// Accumulate per connection: a header/body split across TCP segments is no
	// longer discarded by a single readAll().
	buffer += socket->readAll();
	const auto request = parseRequest( buffer );
	if( request.tooLarge )
	{
		respondError( socket, 413, QByteArrayLiteral("Payload Too Large"), QStringLiteral("request too large") );
		m_buffers.remove( socket );
		return;
	}
	if( request.complete == false ) { return; }
	m_buffers.remove( socket );

	if( request.method == QStringLiteral("OPTIONS") )
	{
		writeResponse( socket, 204, QByteArrayLiteral("No Content"), {}, {} );
		return;
	}

	// Discovery/heartbeat is public metadata only; every data and control route
	// requires a request-bound HMAC over the lesson key.
	const auto path = request.pathAndQuery.section( QLatin1Char('?'), 0, 0 );
	if( path != QStringLiteral("/api/classroom/state") &&
		authorized( socket->peerAddress(), request.headerLines, request.method,
					request.pathAndQuery, request.body ) == false )
	{
		respondDenied( socket );
		return;
	}
	handleRoute( socket, request.method, request.pathAndQuery, request.body );
}


void ClassroomBridge::handleCommand( const QJsonObject& command, bool* handled )
{
	*handled = false;
	const auto name = command.value( QStringLiteral("name") ).toString();
	auto enabled = command.value( QStringLiteral("enabled") ).toBool( true );
	const auto text = command.value( QStringLiteral("text") ).toString().left( 2000 );
	// netsh receives this value as remoteip=. Only a plain unicast IPv4
	// address may survive; 0.0.0.0/0 and friends are dropped here.
	const auto teacherIp = sanitizeTeacherIp( command.value( QStringLiteral("teacherIp") ).toString() );

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
	else
	{
		qWarning( "ikeback: bridge rejected unknown command %s", qPrintable( name ) );
		return;
	}

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
			   .addArgument( Classroom::Argument::Ip, teacherIp )
			   .addArgument( Classroom::Argument::Message, text );
		for( const auto& controlInterface : m_master.allComputerControlInterfaces() )
		{
			controlInterface->sendFeatureMessage( message );
		}
	}
	*handled = true;
}


void ClassroomBridge::serveAsync( QTcpSocket* socket, const std::function<QByteArray()>& work,
								  const QByteArray& contentType )
{
	// The socket is a QPointer: if the phone disconnects while the file scan
	// runs, the (GUI-thread) result slot simply drops the answer. The work
	// itself captures values only, so it cannot outlive this object's members.
	QPointer<QTcpSocket> guarded( socket );
	auto* watcher = new QFutureWatcher<QByteArray>( this );
	connect( watcher, &QFutureWatcher<QByteArray>::finished, this,
			 [this, watcher, guarded, contentType]() {
				 const auto result = watcher->result();
				 watcher->deleteLater();
				 if( guarded.isNull() == false ) { respond( guarded.data(), result, contentType ); }
			 } );
	watcher->setFuture( QtConcurrent::run( work ) );
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
		const auto seat = request.value( QStringLiteral("seat") ).toString().trimmed().left( 16 );
		const auto delta = request.value( QStringLiteral("delta") ).toInt();
		const auto reason = request.value( QStringLiteral("reason") ).toString().left( 200 );
		bool ok = false;
		// One-tap scoring is an adjustment, not an arbitrary rewrite of the book.
		if( seat.isEmpty() == false && validDelta( delta ) && m_className.isEmpty() == false )
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
		const auto className = m_className;
		const auto dataDirectory = m_dataDirectory;
		// Six months of JSONL is far too much work for the GUI thread. The work
		// captures values only and rebuilds a read-only store in the pool.
		serveAsync( socket, [dataDirectory, className, seat]() -> QByteArray {
			ClassroomLogStore store( dataDirectory );
			QStringList lines;
			for( const auto& event : store.load( QDate::currentDate().addDays( -180 ), QDate::currentDate() ) )
			{
				if( event.seat == seat && event.className == className )
				{
					lines.append( QStringLiteral("%1 %2").arg( event.detail, event.app ) );
				}
			}
			return ClassroomWordCloud::toSvg( ClassroomWordCloud::tokenize( lines, {}, 60 ) );
		}, QByteArrayLiteral("image/svg+xml; charset=utf-8") );
		return;
	}
	if( path == QStringLiteral("/api/classroom/events") )
	{
		const auto dataDirectory = m_dataDirectory;
		serveAsync( socket, [dataDirectory]() -> QByteArray {
			ClassroomLogStore store( dataDirectory );
			const auto events = store.load( QDate::currentDate().addDays( -30 ), QDate::currentDate() );
			QJsonArray array;
			const int limit = 2000;
			const int count = static_cast<int>( events.size() );
			for( int index = qMax( 0, count - limit ); index < count; ++index )
			{
				array.append( events.at( index ).toJson() );
			}
			return jsonResponse( QJsonObject{ { QStringLiteral("events"), array } } );
		}, QByteArrayLiteral("application/json; charset=utf-8") );
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
		// Append-only: the phone may contribute new entries, but can never
		// replace the teacher's lessons, rules or existing score entries.
		m_book.load( m_className );
		if( snapshot.isEmpty() == false && m_book.appendSnapshot( snapshot ) )
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

	qWarning( "ikeback: bridge 404 for %s %s", qPrintable( method ), qPrintable( path ) );
	respondError( socket, 404, QByteArrayLiteral("Not Found"), QStringLiteral("unknown route") );
}
