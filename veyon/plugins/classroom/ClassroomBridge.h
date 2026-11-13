/*
 * ClassroomBridge.h - lightweight teacher machine HTTP/JSON bridge for the mobile app
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include "ClassroomScore.h"

#include <functional>

class QJsonArray;
class QTcpServer;
class QTcpSocket;
class QUdpSocket;
class QTimer;
class ClassroomLogStore;
class ClassroomRoster;
class VeyonMasterInterface;

class ClassroomBridge : public QObject
{
	Q_OBJECT
public:
	struct Request
	{
		QString method;
		QString pathAndQuery;
		QStringList headerLines;
		QByteArray body;
		bool complete = false;
		bool tooLarge = false;
	};

	enum Limit
	{
		MaxHeaderBytes = 16 * 1024,
		MaxBodyBytes = 1024 * 1024,
		NonceLifetimeSeconds = 300,
		AuthLockSeconds = 60,
		AuthFailureLimit = 10
	};

	ClassroomBridge( const QString& dataDirectory, ClassroomRoster& roster,
					 ClassroomLogStore& store, const QJsonObject& settings,
					 VeyonMasterInterface& master, QObject* parent = nullptr );

	void setClassName( const QString& className ) { m_className = className; }
	//! Re-read the lesson key after the teacher starts a new lesson.
	void setSettings( const QJsonObject& settings );
	//! Binds to a single LAN address. A null/invalid address is rejected, never
	//! replaced by QHostAddress::Any.
	bool start( int port, const QHostAddress& bindAddress );
	void stop();
	bool isRunning() const;
	QString status() const;
	//! Address the bridge is currently listening on (null when stopped).
	const QHostAddress& bindAddress() const { return m_bindAddress; }
	int port() const { return m_port; }
	void syncNow();

	//! Pure helpers: exercised by ClassroomSelfTest without opening a socket.
	static Request parseRequest( const QByteArray& buffer );
	static QString sanitizeTeacherIp( const QString& value );
	static QHostAddress localUnicastAddress();
	static bool validDelta( int delta ) { return delta >= -5 && delta <= 5 && delta != 0; }

Q_SIGNALS:
	void remoteDataApplied();

private:
	void acceptConnection();
	void readRequest();
	void writeResponse( QTcpSocket* socket, int statusCode, const QByteArray& reason,
						const QByteArray& body, const QByteArray& contentType );
	void respond( QTcpSocket* socket, const QByteArray& body,
				  const QByteArray& contentType = QByteArrayLiteral("application/json; charset=utf-8") );
	void respondDenied( QTcpSocket* socket );
	void respondError( QTcpSocket* socket, int statusCode, const QByteArray& reason, const QString& error );
	void handleRoute( QTcpSocket* socket, const QString& method, const QString& path,
					  const QByteArray& body );
	//! Run a long file scan on the thread pool and answer from the GUI thread.
	void serveAsync( QTcpSocket* socket, const std::function<QByteArray()>& work,
					 const QByteArray& contentType );
	void broadcast();
	void readDiscovery();
	QByteArray announcementPayload();
	qint64 revision() const;
	QJsonObject stateObject();
	QString timetableFilePath() const;
	void handleCommand( const QJsonObject& command, bool* handled );
	bool authorized( const QHostAddress& peer, const QStringList& headerLines, const QString& method,
					 const QString& pathAndQuery, const QByteArray& body );
	QString issueNonce();
	void noteAuthFailure( const QString& peer );
	void purgeStaleNonces();
	void rotateNonce();

	QString m_dataDirectory;
	ClassroomRoster& m_roster;
	ClassroomLogStore& m_store;
	VeyonMasterInterface& m_master;
	QJsonObject m_settings;
	ClassroomScoreBook m_book;
	QString m_className;
	QString m_teacherId;
	QString m_sessionKey;
	QString m_nonce;
	QDateTime m_nonceIssued;

	//! One-time challenges issued by /state. A nonce is removed on first use,
	//! so a captured request cannot be replayed even inside the old 120 s window.
	QHash<QString, QDateTime> m_issuedNonces;
	struct AuthFailure
	{
		int count = 0;
		QDateTime lockUntil;
	};
	//! Failure counting is per source IP: one student machine cannot lock out
	//! every phone on the LAN.
	QHash<QString, AuthFailure> m_authFailures;
	QHash<QTcpSocket*, QByteArray> m_buffers;

	QTcpServer* m_server = nullptr;
	QUdpSocket* m_udp = nullptr;
	QTimer* m_broadcastTimer = nullptr;
	QHostAddress m_bindAddress;
	int m_port = 0;
};
