/*
 * ClassroomSelfTest.cpp - dependency free checks for the classroom data layer
 *
 * Built only with WITH_TESTS=ON via test_veyon_plugin(classroom ClassroomSelfTest).
 * This file is part of the Veyon classroom modification.
 */

#include <QGuiApplication>
#include <QDate>
#include <QDir>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>

#include "ClassroomData.h"
#include "ClassroomProtocol.h"
#include "ClassroomScore.h"
#include "ClassroomSecrets.h"
#include "ClassroomTermReport.h"
#include "ClassroomWordCloud.h"

#include <cstdio>


static int failures = 0;

static void check( bool condition, const char* what )
{
	if( condition == false )
	{
		++failures;
		std::fprintf( stderr, "FAIL: %s\n", what );
	}
	else
	{
		std::fprintf( stdout, "ok  : %s\n", what );
	}
}


int main( int argc, char** argv )
{
	QGuiApplication app( argc, argv );
	QTemporaryDir directory;
	check( directory.isValid(), "temporary data directory" );

	check( ClassroomScoreBook::xmlEscape( QStringLiteral("a<b>&\"") ) ==
		   QStringLiteral("a&lt;b&gt;&amp;&quot;"), "xml escaping" );
	check( ClassroomSecrets::reveal( QStringLiteral("plain-key") ) == QStringLiteral("plain-key"),
		   "legacy plain secrets stay readable" );
	check( ClassroomSecrets::sessionToken( QStringLiteral("ABC123"), QStringLiteral("nonce-0001") ) ==
		   QStringLiteral("e198264293e2ee9d35aade33089e0f33e560cd60143d01292483f9f952330e04"),
		   "lesson-key challenge token matches SHA-256 reference" );
	check( Classroom::announcementPrefix() == QStringLiteral("CLASSKEEP:") &&
		   Classroom::discoveryMagic() == QStringLiteral("CLASSKEEP_DISCOVER"),
		   "mobile discovery constants match the Kotlin app" );
	check( ClassroomSecrets::sessionToken( QStringLiteral("ABC123"), QStringLiteral("nonce-0001") )
			   .contains( QStringLiteral("ABC123") ) == false,
		   "auth token never contains the plain lesson key" );

	const QString imageHtml = QStringLiteral("<img src=\"logo.png\" /> logo.png in text");
	check( ClassroomScoreBook::inlineImages( imageHtml, directory.path() ) ==
		   QStringLiteral("<img src=\"logo.png\" /> logo.png in text"),
		   "missing image is left untouched (no blind string replacement)" );

	ClassroomScoreBook book( directory.path() );
	book.load( QStringLiteral("Class A") );
	ClassroomScoreRules rules;
	rules.baseScore = 100;
	book.setRules( rules );

	// Lessons are inserted out of chronological order on purpose.
	const QString newer = book.ensureLesson( QDate( 2026, 11, 4 ), QStringLiteral("WPS") );
	const QString older = book.ensureLesson( QDate( 2026, 11, 3 ), QStringLiteral("Drawing") );
	check( newer.isEmpty() == false && older.isEmpty() == false && newer != older, "lesson ids" );

	ClassroomScoreEntry late;
	late.lessonId = older;
	late.seat = QStringLiteral("1");
	late.student = QStringLiteral("Zhang");
	late.delta = -5;
	late.label = QStringLiteral("late -5");
	book.addEntry( late );

	ClassroomScoreEntry good;
	good.lessonId = newer;
	good.seat = QStringLiteral("1");
	good.student = QStringLiteral("Zhang");
	good.delta = 3;
	good.label = QStringLiteral("good +3");
	book.addEntry( good );

	check( book.total( QStringLiteral("1"), older ) == 95, "total up to earlier lesson" );
	check( book.total( QStringLiteral("1"), newer ) == 98, "total up to later lesson" );
	check( book.total( QStringLiteral("1") ) == 98, "final total" );
	check( book.entriesForCell( older, QStringLiteral("1") ).size() == 1, "one cell per lesson" );
	check( book.studentHtml( QStringLiteral("1"), QStringLiteral("Zhang") ).
		   contains( QStringLiteral("Scoring rules") ) == false, "unpublished rules are hidden" );

	ClassroomScoreRules published = rules;
	published.published = true;
	published.html = QStringLiteral("<b>Rule</b>");
	book.setRules( published );
	check( book.studentHtml( QStringLiteral("1"), QStringLiteral("Zhang") ).
		   contains( QStringLiteral("<b>Rule</b>") ), "published rules are visible" );

	const auto words = ClassroomWordCloud::tokenize(
		{ QStringLiteral("画图软件 画图软件 画图软件 练习"), QStringLiteral("WPS表格 WPS表格") },
		{ QStringLiteral("画图软件"), QStringLiteral("WPS表格") }, 60 );
	check( words.isEmpty() == false, "word cloud produces tokens" );
	bool hasLongWord = false;
	for( const auto& word : words )
	{
		if( word.text.size() >= 4 ) { hasLongWord = true; }
	}
	check( hasLongWord, "word cloud keeps specific long terms" );
	check( ClassroomWordCloud::toSvg( words ).startsWith( QByteArrayLiteral("<?xml") ), "svg output" );
	check( ClassroomWordCloud::toPixmap( words, 320, 200 ).isNull() == false, "pixmap output" );

	ClassroomRoster roster( directory.path() );
	ClassroomClass classItem;
	classItem.name = QStringLiteral("Class A");
	ClassroomStudent student;
	student.seat = QStringLiteral("1");
	student.name = QStringLiteral("Zhang");
	student.ip = QStringLiteral("192.168.1.10");
	classItem.students.append( student );
	roster.classes().append( classItem );
	roster.save();

	ClassroomTermReport::Options options;
	options.className = QStringLiteral("Class A");
	options.attendanceColumn = true;
	const auto xml = ClassroomTermReport::toSpreadsheetMl( roster, book, options );
	check( xml.contains( QByteArrayLiteral("<?xml") ), "report xml declaration" );
	check( xml.contains( QByteArrayLiteral("</Workbook>") ), "report workbook closed" );
	check( xml.contains( QByteArrayLiteral("98") ), "report contains current total" );
	check( ClassroomTermReport::toCsv( roster, book, options ).startsWith( QByteArrayLiteral("\xEF\xBB\xBF") ),
		   "csv has UTF-8 BOM" );

	std::fprintf( stdout, "\n%d failure(s)\n", failures );
	return failures == 0 ? 0 : 1;
}
