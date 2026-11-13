/*
 * FeatureWorkerManagerConnection.cpp - class which handles communication between service and feature
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Ikeback - https://github.com/zk26/ikeback
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QCoreApplication>
#include <QHostAddress>

#include "FeatureManager.h"
#include "FeatureWorkerManager.h"
#include "FeatureWorkerManagerConnection.h"
#include "IkebackConfiguration.h"


FeatureWorkerManagerConnection::FeatureWorkerManagerConnection(IkebackWorkerInterface& worker,
															   Feature::Uid featureUid,
															   QObject* parent) :
	QObject( parent ),
	m_worker( worker ),
	m_port(IkebackCore::config().featureWorkerManagerPort() + IkebackCore::sessionId()),
	m_socket( this ),
	m_featureUid(featureUid)
{
	connect( &m_connectTimer, &QTimer::timeout, this, &FeatureWorkerManagerConnection::tryConnection );

	connect( &m_socket, &QTcpSocket::connected,
			 this, &FeatureWorkerManagerConnection::sendInitMessage );

	connect(&m_socket, &QTcpSocket::disconnected, this,
			[=]() {
		vDebug() << "lost connection to FeatureWorkerManager – exiting";
		QCoreApplication::instance()->exit(0);
	}, Qt::QueuedConnection);

	connect( &m_socket, &QTcpSocket::readyRead,
			 this, &FeatureWorkerManagerConnection::receiveMessage );

	// ikeback 5.2.0: without this bound, a worker whose service is gone (or
	// whose stdin token was lost) retries every 3 s forever. Dozens of idle
	// ikeback-worker.exe processes then accumulate in Task Manager.
	QTimer::singleShot( StartupTimeout, this, [this]() {
		if( m_socket.state() != QAbstractSocket::ConnectedState || m_authToken.isEmpty() )
		{
			vWarning() << "feature worker" << m_featureUid
					   << "did not authenticate within" << StartupTimeout << "ms - exiting";
			QCoreApplication::instance()->exit( 1 );
		}
	} );

	tryConnection();
}



void FeatureWorkerManagerConnection::setAuthToken(const QByteArray& authToken)
{
	if (m_authToken.isEmpty())
	{
		m_authToken = authToken;

		if (m_socket.state() == QTcpSocket::ConnectedState)
		{
			sendInitMessage();
		}
	}
	else
	{
		vCritical() << "auth token already set";
	}
}



bool FeatureWorkerManagerConnection::sendMessage( const FeatureMessage& message )
{
	vDebug() << message;

	return message.sendPlain(&m_socket);
}



void FeatureWorkerManagerConnection::tryConnection()
{
	if( m_socket.state() != QTcpSocket::ConnectedState )
	{
		vDebug() << "connecting to FeatureWorkerManager at port" << m_port;

		m_socket.connectToHost(QHostAddress::LocalHost, m_port);
		m_connectTimer.start(ConnectTimeout);
	}
}



void FeatureWorkerManagerConnection::sendInitMessage()
{
	m_connectTimer.stop();

	if (m_authToken.isEmpty() == false)
	{
		vDebug() << m_featureUid;

		FeatureMessage(m_featureUid, FeatureMessage::Command::Init)
				.addArgument(FeatureWorkerManager::MessageArgument::AuthToken, m_authToken)
				.sendPlain(&m_socket);
	}
}



void FeatureWorkerManagerConnection::receiveMessage()
{
	FeatureMessage featureMessage;

	while( featureMessage.isReadyForReceive( &m_socket ) )
	{
		if( featureMessage.receive( &m_socket ) )
		{
			IkebackCore::featureManager().handleFeatureMessage( m_worker, featureMessage );
		}
	}
}
