/*
 * ClassroomBridge.h - lightweight teacher machine HTTP/JSON bridge for the mobile app
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include "ClassroomScore.h"

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
	ClassroomBridge( const QString& dataDirectory, ClassroomRoster& roster,
					 ClassroomLogStore& store, const QJsonObject& settings,
					 VeyonMasterInterface& master, QObject* parent = nullptr );

	void setClassName( const QString& className ) { m_className = className; }
	//! Re-read the lesson key after the teacher starts a new lesson.
	void setSettings( const QJsonObject& settings );
	bool start( int port );
	void stop();
	bool isRunning() const;
	QString status() const;
	void syncNow();

Q_SIGNALS:
	void remoteDataApplied();

private:
	void acceptConnection();
	void readRequest();
	void respond( QTcpSocket* socket, const QByteArray& body,
				  const QByteArray& contentType = QByteArrayLiteral("application/json; charset=utf-8") );
	void respondDenied( QTcpSocket* socket );
	void handleRoute( QTcpSocket* socket, const QString& method, const QString& path,
					  const QByteArray& body );
	void broadcast();
	void readDiscovery();
	QByteArray announcementPayload();
	qint64 revision() const;
	QJsonObject stateObject() const;
	QString timetableFilePath() const;
	void handleCommand( const QJsonObject& command, bool* handled );
	QString tokenFor( const QString& nonce ) const;
	bool authorized( const QStringList& headerLines ) const;
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
	QString m_previousNonce;
	QDateTime m_nonceIssued;
	mutable int m_failedAuths = 0;
	mutable QDateTime m_authLockUntil;

	QTcpServer* m_server = nullptr;
	QUdpSocket* m_udp = nullptr;
	QTimer* m_broadcastTimer = nullptr;
	int m_port = 0;
};
