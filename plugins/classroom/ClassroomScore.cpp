/*
 * ClassroomScore.cpp - lesson based scoring with rich text details
 *
 * This file is part of the Veyon classroom modification.
 */

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPair>
#include <QRegularExpression>
#include <QStringList>
#include <QUuid>

#include "ClassroomData.h"
#include "ClassroomScore.h"


static QString trText( const char* text )
{
	return QCoreApplication::translate( "ClassroomScoreBook", text );
}


static QJsonArray toLessonArray( const QVector<ClassroomScoreLesson>& lessons )
{
	QJsonArray array;
	for( const auto& lesson : lessons ) { array.append( lesson.toJson() ); }
	return array;
}


static QJsonArray toEntryArray( const QVector<ClassroomScoreEntry>& entries )
{
	QJsonArray array;
	for( const auto& entry : entries ) { array.append( entry.toJson() ); }
	return array;
}


static QJsonArray toQuickActionArray( const QVector<QPair<QString, int>>& actions )
{
	QJsonArray array;
	for( const auto& action : actions )
	{
		array.append( QJsonArray{ action.first, action.second } );
	}
	return array;
}


QJsonObject ClassroomScoreLesson::toJson() const
{
	return { { QStringLiteral("id"), id },
			 { QStringLiteral("date"), date.toString( Qt::ISODate ) },
			 { QStringLiteral("topic"), topic } };
}


ClassroomScoreLesson ClassroomScoreLesson::fromJson( const QJsonObject& object )
{
	ClassroomScoreLesson lesson;
	lesson.id = object.value( QStringLiteral("id") ).toString();
	lesson.date = QDate::fromString( object.value( QStringLiteral("date") ).toString(), Qt::ISODate );
	lesson.topic = object.value( QStringLiteral("topic") ).toString();
	return lesson;
}


QJsonObject ClassroomScoreEntry::toJson() const
{
	return { { QStringLiteral("id"), id },
			 { QStringLiteral("timestamp"), timestamp.toString( Qt::ISODateWithMs ) },
			 { QStringLiteral("lessonId"), lessonId },
			 { QStringLiteral("className"), className },
			 { QStringLiteral("seat"), seat },
			 { QStringLiteral("student"), student },
			 { QStringLiteral("delta"), delta },
			 { QStringLiteral("label"), label },
			 { QStringLiteral("richText"), richText },
			 { QStringLiteral("author"), author } };
}


ClassroomScoreEntry ClassroomScoreEntry::fromJson( const QJsonObject& object )
{
	ClassroomScoreEntry entry;
	entry.id = object.value( QStringLiteral("id") ).toString();
	entry.timestamp = QDateTime::fromString( object.value( QStringLiteral("timestamp") ).toString(), Qt::ISODateWithMs );
	entry.lessonId = object.value( QStringLiteral("lessonId") ).toString();
	entry.className = object.value( QStringLiteral("className") ).toString();
	entry.seat = object.value( QStringLiteral("seat") ).toString();
	entry.student = object.value( QStringLiteral("student") ).toString();
	entry.delta = object.value( QStringLiteral("delta") ).toInt();
	entry.label = object.value( QStringLiteral("label") ).toString();
	entry.richText = object.value( QStringLiteral("richText") ).toString();
	entry.author = object.value( QStringLiteral("author") ).toString();
	return entry;
}


QJsonObject ClassroomScoreRules::toJson() const
{
	return { { QStringLiteral("published"), published },
			 { QStringLiteral("baseScore"), baseScore },
			 { QStringLiteral("html"), html },
			 { QStringLiteral("updated"), updated.toString( Qt::ISODateWithMs ) },
			 { QStringLiteral("quickActions"), toQuickActionArray( quickActions ) } };
}


ClassroomScoreRules ClassroomScoreRules::fromJson( const QJsonObject& object )
{
	ClassroomScoreRules rules;
	rules.published = object.value( QStringLiteral("published") ).toBool( false );
	rules.baseScore = object.value( QStringLiteral("baseScore") ).toInt( 100 );
	rules.html = object.value( QStringLiteral("html") ).toString();
	rules.updated = QDateTime::fromString( object.value( QStringLiteral("updated") ).toString(), Qt::ISODateWithMs );
	const auto actions = object.value( QStringLiteral("quickActions") ).toArray();
	for( const auto& action : actions )
	{
		const auto pair = action.toArray();
		if( pair.size() >= 2 )
		{
			rules.quickActions.append( { pair.at( 0 ).toString(), pair.at( 1 ).toInt() } );
		}
	}
	return rules;
}


ClassroomScoreBook::ClassroomScoreBook( const QString& dataDirectory ) :
	m_basePath( dataDirectory )
{
}


QString ClassroomScoreBook::filePath( const QString& className ) const
{
	const QString directory = QDir( m_basePath ).filePath( QStringLiteral("scores") );
	return QDir( directory ).filePath( ClassroomLogStore::sanitize( className ) + QStringLiteral(".json") );
}


bool ClassroomScoreBook::load( const QString& className )
{
	m_className = className;
	m_lessons.clear();
	m_entries.clear();
	m_rules = ClassroomScoreRules{};

	const auto fileName = filePath( className );
	QFile file( fileName );
	if( file.exists() == false ) { return true; }
	if( file.open( QFile::ReadOnly ) == false ) { return false; }

	const auto document = QJsonDocument::fromJson( file.readAll() );
	const auto object = document.object();
	m_rules = ClassroomScoreRules::fromJson( object.value( QStringLiteral("rules") ).toObject() );
	const auto lessons = object.value( QStringLiteral("lessons") ).toArray();
	for( const auto& value : lessons ) { m_lessons.append( ClassroomScoreLesson::fromJson( value.toObject() ) ); }
	const auto entries = object.value( QStringLiteral("entries") ).toArray();
	for( const auto& value : entries ) { m_entries.append( ClassroomScoreEntry::fromJson( value.toObject() ) ); }
	return true;
}


bool ClassroomScoreBook::save() const
{
	const auto fileName = filePath( m_className );
	if( QDir().mkpath( QFileInfo( fileName ).absolutePath() ) == false ) { return false; }
	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false ) { return false; }
	const QByteArray data = QJsonDocument( syncSnapshot() ).toJson( QJsonDocument::Indented );
	return file.write( data ) == data.size();
}


bool ClassroomScoreBook::setRules( const ClassroomScoreRules& rules )
{
	m_rules = rules;
	m_rules.updated = QDateTime::currentDateTime();
	return save();
}


QString ClassroomScoreBook::ensureLesson( const QDate& date, const QString& topic )
{
	for( const auto& lesson : m_lessons )
	{
		if( lesson.date == date && lesson.topic == topic ) { return lesson.id; }
	}

	ClassroomScoreLesson lesson;
	lesson.id = QUuid::createUuid().toString( QUuid::WithoutBraces ).left( 12 );
	lesson.date = date;
	lesson.topic = topic;
	m_lessons.append( lesson );
	save();
	return lesson.id;
}


bool ClassroomScoreBook::addEntry( const ClassroomScoreEntry& entry )
{
	ClassroomScoreEntry copy = entry;
	if( copy.id.isEmpty() ) { copy.id = QUuid::createUuid().toString( QUuid::WithoutBraces ); }
	if( copy.timestamp.isValid() == false ) { copy.timestamp = QDateTime::currentDateTime(); }
	if( copy.className.isEmpty() ) { copy.className = m_className; }
	if( copy.richText.isEmpty() ) { copy.richText = xmlEscape( copy.label ); }
	m_entries.append( copy );
	return save();
}


bool ClassroomScoreBook::removeEntry( const QString& entryId )
{
	for( int index = 0; index < m_entries.size(); ++index )
	{
		if( m_entries.at( index ).id == entryId )
		{
			m_entries.removeAt( index );
			return save();
		}
	}
	return false;
}


int ClassroomScoreBook::total( const QString& seat, const QString& upToLessonId ) const
{
	auto ordered = m_lessons;
	std::stable_sort( ordered.begin(), ordered.end(),
					  []( const ClassroomScoreLesson& a, const ClassroomScoreLesson& b ) {
						  return a.date < b.date;
					  } );

	int lastLesson = ordered.size() - 1;
	if( upToLessonId.isEmpty() == false )
	{
		lastLesson = -1;
		for( int index = 0; index < ordered.size(); ++index )
		{
			if( ordered.at( index ).id == upToLessonId ) { lastLesson = index; break; }
		}
	}

	int score = m_rules.baseScore;
	for( const auto& entry : m_entries )
	{
		if( entry.seat != seat || entry.lessonId.isEmpty() ) { continue; }
		for( int index = 0; index <= lastLesson && index < ordered.size(); ++index )
		{
			if( ordered.at( index ).id == entry.lessonId )
			{
				score += entry.delta;
				break;
			}
		}
	}
	return score;
}


QVector<ClassroomScoreEntry> ClassroomScoreBook::entriesForCell( const QString& lessonId, const QString& seat ) const
{
	QVector<ClassroomScoreEntry> result;
	for( const auto& entry : m_entries )
	{
		if( entry.lessonId == lessonId && entry.seat == seat ) { result.append( entry ); }
	}
	return result;
}


QVector<ClassroomScoreEntry> ClassroomScoreBook::entriesForStudent( const QString& seat ) const
{
	QVector<ClassroomScoreEntry> result;
	for( const auto& entry : m_entries )
	{
		if( entry.seat == seat ) { result.append( entry ); }
	}
	return result;
}


QString ClassroomScoreBook::studentHtml( const QString& seat, const QString& student ) const
{
	const auto score = total( seat );
	const auto title = student.isEmpty() ? seat : student;

	QString html;
	html += QStringLiteral("<html><body style=\"font-family:'Microsoft YaHei',sans-serif;\">");
	html += QStringLiteral("<h2>%1</h2>").arg( xmlEscape( title ) );
	html += QStringLiteral("<p><b>%1 %2</b> &nbsp; <span style=\"color:#888;\">%3 %4</span></p>")
				.arg( trText("Current score" ), QString::number( score ),
					  trText("Base score" ), QString::number( m_rules.baseScore ) );

	html += QStringLiteral("<hr><h3>%1</h3>").arg( trText("Score details" ) );
	const auto details = entriesForStudent( seat );
	if( details.isEmpty() )
	{
		html += QStringLiteral("<p style=\"color:#888;\">%1</p>").arg( trText("No entries yet." ) );
	}
	else
	{
		html += QStringLiteral("<table width=\"100%\" border=\"1\" cellspacing=\"0\" cellpadding=\"4\">");
		html += QStringLiteral("<tr><th>%1</th><th>%2</th><th>%3</th><th>%4</th></tr>")
					.arg( trText("Time" ), trText("Lesson" ), trText("Detail" ), trText("Delta" ) );
		for( const auto& entry : details )
		{
			QString topic;
			for( const auto& lesson : m_lessons )
			{
				if( lesson.id == entry.lessonId )
				{
					topic = lesson.date.toString( QStringLiteral("MM.dd") ) + QLatin1Char(' ') + lesson.topic;
					break;
				}
			}
			html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
						.arg( entry.timestamp.toString( QStringLiteral("yyyy-MM-dd HH:mm") ),
							  xmlEscape( topic ), inlineImages( entry.richText, m_basePath ),
							  entry.delta > 0 ? QStringLiteral("+%1").arg( entry.delta ) : QString::number( entry.delta ) );
		}
		html += QStringLiteral("</table>");
	}

	if( m_rules.published )
	{
		html += QStringLiteral("<hr><h3>%1</h3>").arg( trText("Scoring rules" ) );
		html += inlineImages( m_rules.html, m_basePath );
	}
	return QStringLiteral("%1</body></html>").arg( html );
}


QJsonObject ClassroomScoreBook::syncSnapshot() const
{
	return { { QStringLiteral("className"), m_className },
			 { QStringLiteral("rules"), m_rules.toJson() },
			 { QStringLiteral("lessons"), toLessonArray( m_lessons ) },
			 { QStringLiteral("entries"), toEntryArray( m_entries ) } };
}


bool ClassroomScoreBook::applySnapshot( const QJsonObject& snapshot )
{
	const auto className = snapshot.value( QStringLiteral("className") ).toString();
	if( className.isEmpty() ) { return false; }
	m_className = className;
	m_rules = ClassroomScoreRules::fromJson( snapshot.value( QStringLiteral("rules") ).toObject() );
	m_lessons.clear();
	const auto lessons = snapshot.value( QStringLiteral("lessons") ).toArray();
	for( const auto& value : lessons ) { m_lessons.append( ClassroomScoreLesson::fromJson( value.toObject() ) ); }
	m_entries.clear();
	const auto entries = snapshot.value( QStringLiteral("entries") ).toArray();
	for( const auto& value : entries ) { m_entries.append( ClassroomScoreEntry::fromJson( value.toObject() ) ); }
	return save();
}


QString ClassroomScoreBook::inlineImages( const QString& html, const QString& dataDirectory )
{
	const QRegularExpression pattern( QStringLiteral("<img[^>]+src=[\"']([^\"']+)[\"']"),
									  QRegularExpression::CaseInsensitiveOption );
	QString result;
	int cursor = 0;
	auto it = pattern.globalMatch( html );
	while( it.hasNext() )
	{
		const auto match = it.next();
		const auto source = match.captured( 1 );
		const int sourceStart = match.capturedStart( 1 );
		const int sourceEnd = match.capturedEnd( 1 );

		QString uri;
		if( source.startsWith( QStringLiteral("data:"), Qt::CaseInsensitive ) == false )
		{
			QString fileName = source;
			if( fileName.startsWith( QStringLiteral("file:///") ) ) { fileName.remove( 0, 8 ); }
			if( QFileInfo( fileName ).isRelative() )
			{
				fileName = QDir( dataDirectory ).filePath( fileName );
			}
			QFile image( fileName );
			if( image.open( QFile::ReadOnly ) )
			{
				const auto bytes = image.readAll();
				const auto mime = fileName.endsWith( QStringLiteral(".png"), Qt::CaseInsensitive ) ?
									  QStringLiteral("image/png") : QStringLiteral("image/jpeg");
				uri = QStringLiteral("data:%1;base64,%2").arg( mime, QString::fromLatin1( bytes.toBase64() ) );
			}
		}

		result += html.mid( cursor, sourceStart - cursor );
		result += uri.isEmpty() ? source : uri;
		cursor = sourceEnd;
	}
	result += html.mid( cursor );
	return result;
}


QString ClassroomScoreBook::xmlEscape( const QString& text )
{
	QString result = text;
	result.replace( QLatin1Char('&'), QStringLiteral("&amp;") );
	result.replace( QLatin1Char('<'), QStringLiteral("&lt;") );
	result.replace( QLatin1Char('>'), QStringLiteral("&gt;") );
	result.replace( QLatin1Char('"'), QStringLiteral("&quot;") );
	return result;
}
