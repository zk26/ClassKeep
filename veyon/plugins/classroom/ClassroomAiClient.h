/*
 * ClassroomAiClient.h - minimal OpenAI-compatible HTTP chat client
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;

class ClassroomAiClient : public QObject
{
	Q_OBJECT
public:
	struct Settings
	{
		bool enabled = true;
		QString endpoint;
		QString apiKey;
		QString model;
		QString systemPrompt;
		int timeout = 60;

		static Settings fromJson( const QJsonObject& object, const Settings& fallback = Settings{} );
		QJsonObject toJson() const;
	};

	explicit ClassroomAiClient( QObject* parent = nullptr );

	void setSettings( const Settings& settings );
	const Settings& settings() const { return m_settings; }

	void chat( const QVariantList& history, const QString& prompt );
	void abort();

Q_SIGNALS:
	void finished( bool success, const QString& reply );

private:
	Settings m_settings;
	QNetworkAccessManager* m_network = nullptr;
	QPointer<QNetworkReply> m_reply;
};
