/*
 * ClassroomScore.h - lesson based scoring with rich text details
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <QVector>

struct ClassroomScoreLesson
{
	QString id;
	QDate date;
	QString topic;

	QJsonObject toJson() const;
	static ClassroomScoreLesson fromJson( const QJsonObject& object );
};

struct ClassroomScoreEntry
{
	QString id;
	QDateTime timestamp;
	QString lessonId;
	QString className;
	QString seat;
	QString student;
	int delta = 0;
	QString label;
	QString richText;
	QString author;

	QJsonObject toJson() const;
	static ClassroomScoreEntry fromJson( const QJsonObject& object );
};

struct ClassroomScoreRules
{
	bool published = false;
	int baseScore = 100;
	QString html;
	QDateTime updated;
	QVector<QPair<QString, int>> quickActions;

	QJsonObject toJson() const;
	static ClassroomScoreRules fromJson( const QJsonObject& object );
};

class ClassroomScoreBook
{
public:
	explicit ClassroomScoreBook( const QString& dataDirectory );

	QString filePath( const QString& className ) const;
	bool load( const QString& className );
	bool save() const;

	const QString& className() const { return m_className; }
	const QVector<ClassroomScoreLesson>& lessons() const { return m_lessons; }
	const QVector<ClassroomScoreEntry>& entries() const { return m_entries; }
	const ClassroomScoreRules& rules() const { return m_rules; }

	bool setRules( const ClassroomScoreRules& rules );
	QString ensureLesson( const QDate& date, const QString& topic );
	bool addEntry( const ClassroomScoreEntry& entry );
	bool removeEntry( const QString& entryId );

	int total( const QString& seat, const QString& upToLessonId = QString{} ) const;
	QVector<ClassroomScoreEntry> entriesForCell( const QString& lessonId, const QString& seat ) const;
	QVector<ClassroomScoreEntry> entriesForStudent( const QString& seat ) const;

	QString studentHtml( const QString& seat, const QString& student ) const;
	QJsonObject syncSnapshot() const;
	//! Teacher-side full replace. Never call this with a phone-supplied body.
	bool applySnapshot( const QJsonObject& snapshot );
	//! Phone-side append: only new, well-formed entries are accepted; rules,
	//! lessons and existing entries are never modified or removed.
	bool appendSnapshot( const QJsonObject& snapshot );

	static QString inlineImages( const QString& html, const QString& dataDirectory );
	static QString xmlEscape( const QString& text );

private:
	QString m_basePath;
	QString m_className;
	QVector<ClassroomScoreLesson> m_lessons;
	QVector<ClassroomScoreEntry> m_entries;
	ClassroomScoreRules m_rules;
};
