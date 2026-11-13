/*
 * ClassroomAiClient.cpp - minimal OpenAI-compatible HTTP chat client
 *
 * This file is part of the Ikeback classroom modification.
 */

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include "ClassroomAiClient.h"
#include "ClassroomProtocol.h"


ClassroomAiClient::Settings ClassroomAiClient::Settings::fromJson( const QJsonObject& object, const Settings& fallback )
{
	Settings settings = fallback;
	const auto readString = [&object]( const QString& key, const QString& defaultValue ) {
		return object.contains( key ) ? object.value( key ).toString() : defaultValue;
	};

	settings.enabled = object.contains( Classroom::SettingKey::aiEnabled() ) ?
						   object.value( Classroom::SettingKey::aiEnabled() ).toBool() : fallback.enabled;
	settings.endpoint = readString( Classroom::SettingKey::aiEndpoint(), fallback.endpoint );
	settings.apiKey = readString( Classroom::SettingKey::aiApiKey(), fallback.apiKey );
	settings.model = readString( Classroom::SettingKey::aiModel(), fallback.model );
	settings.systemPrompt = readString( Classroom::SettingKey::aiSystemPrompt(), fallback.systemPrompt );
	settings.timeout = object.contains( Classroom::SettingKey::aiTimeout() ) ?
						   object.value( Classroom::SettingKey::aiTimeout() ).toInt() : fallback.timeout;
	return settings;
}


QJsonObject ClassroomAiClient::Settings::toJson() const
{
	return { { Classroom::SettingKey::aiEnabled(), enabled },
			 { Classroom::SettingKey::aiEndpoint(), endpoint },
			 { Classroom::SettingKey::aiApiKey(), apiKey },
			 { Classroom::SettingKey::aiModel(), model },
			 { Classroom::SettingKey::aiSystemPrompt(), systemPrompt },
			 { Classroom::SettingKey::aiTimeout(), timeout } };
}


ClassroomAiClient::ClassroomAiClient( QObject* parent ) :
	QObject( parent ),
	m_network( new QNetworkAccessManager( this ) )
{
}


void ClassroomAiClient::setSettings( const Settings& settings )
{
	m_settings = settings;
}


void ClassroomAiClient::abort()
{
	if( m_reply )
	{
		m_reply->abort();
		m_reply->deleteLater();
		m_reply.clear();
	}
}


void ClassroomAiClient::chat( const QVariantList& history, const QString& prompt )
{
	if( m_proxyMode )
	{
		// The teacher machine owns the API key. Forward only the prompt and
		// the short chat history over Ikeback's existing feature channel.
		if( m_settings.enabled == false || prompt.trimmed().isEmpty() )
		{
			Q_EMIT finished( false, tr( "AI is not enabled or the message is empty." ) );
			return;
		}
		Q_EMIT promptRequested( prompt.left( 4000 ), history );
		return;
	}

	if( m_settings.enabled == false || m_settings.endpoint.isEmpty() || prompt.trimmed().isEmpty() )
	{
		Q_EMIT finished( false, tr( "AI is not configured or the message is empty." ) );
		return;
	}

	abort();

	QJsonArray messages;
	if( m_settings.systemPrompt.trimmed().isEmpty() == false )
	{
		messages.append( QJsonObject{ { QStringLiteral("role"), QStringLiteral("system") },
									  { QStringLiteral("content"), m_settings.systemPrompt } } );
	}
	for( const auto& item : history )
	{
		const auto entry = item.toMap();
		const auto role = entry.value( QStringLiteral("role") ).toString();
		const auto content = entry.value( QStringLiteral("content") ).toString();
		if( ( role == QStringLiteral("user") || role == QStringLiteral("assistant") ) && content.isEmpty() == false )
		{
			messages.append( QJsonObject{ { QStringLiteral("role"), role }, { QStringLiteral("content"), content } } );
		}
	}
	messages.append( QJsonObject{ { QStringLiteral("role"), QStringLiteral("user") },
								  { QStringLiteral("content"), prompt } } );

	QJsonObject payload{ { QStringLiteral("model"), m_settings.model },
						 { QStringLiteral("messages"), messages },
						 { QStringLiteral("stream"), false } };

	QNetworkRequest request{ QUrl( m_settings.endpoint ) };
	request.setHeader( QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json") );
	if( m_settings.apiKey.isEmpty() == false )
	{
		request.setRawHeader( QByteArrayLiteral("Authorization"),
							  QStringLiteral("Bearer %1").arg( m_settings.apiKey ).toUtf8() );
	}

	m_reply = m_network->post( request, QJsonDocument( payload ).toJson( QJsonDocument::Compact ) );
	if( m_reply == nullptr )
	{
		Q_EMIT finished( false, tr( "Could not send the request." ) );
		return;
	}

	QTimer::singleShot( std::max( 5, m_settings.timeout ) * 1000, m_reply.data(), [this]() {
		if( m_reply && m_reply->isFinished() == false )
		{
			m_reply->abort();
		}
	} );

	connect( m_reply.data(), &QNetworkReply::finished, this, [this]() {
		auto* reply = m_reply.data();
		if( reply == nullptr )
		{
			Q_EMIT finished( false, tr( "The request was cancelled." ) );
			return;
		}

		const auto data = reply->readAll();
		const auto error = reply->error();
		reply->deleteLater();
		m_reply.clear();

		if( error != QNetworkReply::NoError )
		{
			Q_EMIT finished( false, tr( "AI request failed: %1" ).arg( reply->errorString() ) );
			return;
		}

		const auto document = QJsonDocument::fromJson( data );
		const auto object = document.object();
		QString answer;
		const auto choices = object.value( QStringLiteral("choices") ).toArray();
		if( choices.isEmpty() == false )
		{
			answer = choices.first().toObject().value( QStringLiteral("message") ).toObject().
					 value( QStringLiteral("content") ).toString();
			if( answer.isEmpty() )
			{
				answer = choices.first().toObject().value( QStringLiteral("text") ).toString();
			}
		}
		if( answer.isEmpty() ) { answer = object.value( QStringLiteral("message") ).toObject().value( QStringLiteral("content") ).toString(); }
		if( answer.isEmpty() ) { answer = object.value( QStringLiteral("response") ).toString(); }
		if( answer.isEmpty() ) { answer = object.value( QStringLiteral("content") ).toString(); }

		if( answer.isEmpty() )
		{
			Q_EMIT finished( false, tr( "The AI endpoint returned an unexpected response." ) );
			return;
		}

		Q_EMIT finished( true, answer.trimmed() );
	} );
}
