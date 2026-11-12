/*
 * ClassroomData.h - roster, per-machine event logs and interest analysis
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class ClassroomConfiguration;

class ClassroomSettings
{
public:
	ClassroomSettings( const QString& dataDirectory, const ClassroomConfiguration& configuration );

	static QJsonObject fromConfiguration( const ClassroomConfiguration& configuration );

	//! Settings that may be sent to a student machine. NEVER contains the bridge
	//! key, the AI API key, or any other teacher-side credential; instead it
	//! carries a boolean telling the worker whether a lesson has been started.
	static QJsonObject forStudents( const QJsonObject& object );

	//! Rewrite a pre-2.3 settings.json (enc:) to the user-bound DPAPI format.
	bool migrateSecrets();

	QString filePath() const { return m_filePath; }
	const QJsonObject& object() const { return m_object; }
	void setObject( const QJsonObject& object ) { m_object = object; }

	bool load();
	bool save() const;

private:
	QString m_filePath;
	QJsonObject m_object;
};


class ClassroomStudent
{
public:
	QString seat;
	QString name;
	QString ip;

	QJsonObject toJson() const;
	static ClassroomStudent fromJson( const QJsonObject& object );
};

class ClassroomClass
{
public:
	QString name;
	QVector<ClassroomStudent> students;

	QJsonObject toJson() const;
	static ClassroomClass fromJson( const QJsonObject& object );
};

class ClassroomRoster
{
public:
	explicit ClassroomRoster( const QString& dataDirectory );

	QString filePath() const;
	bool load();
	bool save() const;
	void clear();

	const QVector<ClassroomClass>& classes() const { return m_classes; }
	QVector<ClassroomClass>& classes() { return m_classes; }
	QStringList classNames() const;
	ClassroomClass* findClass( const QString& name );
	const ClassroomClass* findClass( const QString& name ) const;
	ClassroomStudent* findStudent( const QString& className, const QString& seat );
	ClassroomStudent* findStudentByIp( const QString& className, const QString& ip );

	QString toJson() const;
	QJsonArray toJsonArray() const;
	void fromJson( const QString& json );

	bool importCsv( const QString& fileName, const QString& className, QString* error = nullptr );
	bool exportCsv( const QString& fileName, const QString& className, QString* error = nullptr ) const;

private:
	QString m_filePath;
	QVector<ClassroomClass> m_classes;
};

class ClassroomEvent
{
public:
	QDateTime timestamp;
	QString sessionId;
	QString className;
	QString seat;
	QString student;
	QString host;
	QString ip;
	QString type;
	QString app;
	QString detail;
	QString category;

	QJsonObject toJson() const;
	static ClassroomEvent fromJson( const QJsonObject& object );
};

class ClassroomLogStore
{
public:
	explicit ClassroomLogStore( const QString& baseDirectory, const QString& scope = QStringLiteral("teacher") );

	QString basePath() const;
	QString eventsPath() const;
	QString machinesPath() const;
	QString studentsPath() const;

	bool append( const ClassroomEvent& event );
	QVector<ClassroomEvent> load( const QDate& from, const QDate& to, const QString& machine = QString{} ) const;
	QStringList machines() const;
	int purgeExpired( int days );

	static bool exportCsv( const QString& fileName, const QVector<ClassroomEvent>& events, QString* error = nullptr );
	static bool exportProfilesCsv( const QString& fileName, const QStringList& header,
								   const QVector<QStringList>& rows, QString* error = nullptr );

	static QString sanitize( const QString& name );

	//! In-memory identity/last-activity index so the UI does not reparse every log file on each refresh.
	QMap<QString, QString> identityStates() const;
	QMap<QString, QDateTime> lastActivities() const;

private:
	static QVector<ClassroomEvent> readDayFile( const QString& fileName );
	static bool writeLine( const QString& fileName, const QByteArray& line );
	void ensureIndex() const;
	void indexEvent( const ClassroomEvent& event ) const;

	QString m_basePath;
	mutable QMutex m_mutex;
	mutable QMap<QString, QString> m_identityStates;
	mutable QMap<QString, QDateTime> m_lastActivities;
	mutable bool m_indexLoaded = false;
	mutable QSet<QString> m_knownDirectories;
};

struct ClassroomInterestCategory
{
	QString name;
	QStringList keywords;
	QString layer;
};

struct ClassroomInterestProfile
{
	QString key;
	QString className;
	QString seat;
	QString student;
	int sessions = 0;
	int events = 0;
	QMap<QString, double> scores;
	QString topCategory;
	QString suggestion;
	double trend = 0.0;
};

class ClassroomInterestAnalyzer
{
public:
	static QString defaultCategoriesJson();
	static QVector<ClassroomInterestCategory> categoriesFromJson( const QString& json );
	static QString categoriesToJson( const QVector<ClassroomInterestCategory>& categories );
	static QString categorize( const QString& text, const QVector<ClassroomInterestCategory>& categories );

	static QVector<ClassroomInterestProfile> analyze( const QVector<ClassroomEvent>& events,
													  const QVector<ClassroomInterestCategory>& categories,
													  int halfLifeDays = 30 );
};
