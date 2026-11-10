/*
 * ClassroomTermReport.cpp - end of term class report matching the teacher's spreadsheet
 *
 * This file is part of the Veyon classroom modification.
 */

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include "ClassroomData.h"
#include "ClassroomScore.h"
#include "ClassroomTermReport.h"


static QString trText( const char* text )
{
	return QCoreApplication::translate( "ClassroomTermReport", text );
}


static QString cellText( const QString& text )
{
	QString result = ClassroomScoreBook::xmlEscape( text );
	result.replace( QLatin1Char('\n'), QStringLiteral("&#10;") );
	return result;
}


static QString sheetName( const QString& name )
{
	QString result = name.trimmed();
	const QString invalid = QStringLiteral(":\\/?*[]");
	for( const auto character : invalid )
	{
		result.replace( character, QLatin1Char('_') );
	}
	if( result.isEmpty() ) { result = QStringLiteral("Sheet"); }
	if( result.size() > 31 ) { result.truncate( 31 ); }
	return ClassroomScoreBook::xmlEscape( result );
}


static QVector<ClassroomScoreLesson> filteredLessons( const ClassroomScoreBook& book,
													  const ClassroomTermReport::Options& options )
{
	QVector<ClassroomScoreLesson> lessons;
	for( const auto& lesson : book.lessons() )
	{
		if( options.from.isValid() && lesson.date < options.from ) { continue; }
		if( options.to.isValid() && lesson.date > options.to ) { continue; }
		lessons.append( lesson );
	}
	return lessons;
}


static QString lessonHeader( const ClassroomScoreLesson& lesson )
{
	const QString date = lesson.date.toString( QStringLiteral("M.d") );
	return QStringLiteral("%1 %2").arg( date, lesson.topic );
}


static QString cellContent( const ClassroomScoreBook& book, const QString& lessonId, const QString& seat )
{
	const auto entries = book.entriesForCell( lessonId, seat );
	QStringList lines;
	for( const auto& entry : entries )
	{
		lines.append( entry.label );
	}
	if( lines.isEmpty() ) { return {}; }
	return lines.join( QLatin1Char('\n') );
}


static int cellDelta( const ClassroomScoreBook& book, const QString& lessonId, const QString& seat )
{
	int delta = 0;
	for( const auto& entry : book.entriesForCell( lessonId, seat ) ) { delta += entry.delta; }
	return delta;
}


static QString attendanceMark( const ClassroomScoreBook& book, const QString& lessonId, const QString& seat )
{
	QString mark = QStringLiteral("\u221A");
	for( const auto& entry : book.entriesForCell( lessonId, seat ) )
	{
		const auto& label = entry.label;
		if( label.contains( QStringLiteral("\u7F3A\u52E4") ) ) { return QStringLiteral("\u7F3A\u52E4"); }
		if( label.contains( QStringLiteral("\u8BF7\u5047") ) ) { return QStringLiteral("\u8BF7\u5047"); }
		if( label.contains( QStringLiteral("\u8FDF\u5230") ) ) { mark = QStringLiteral("\u8FDF\u5230"); }
	}
	return mark;
}


namespace
{
struct DetailRow
{
	QDateTime timestamp;
	QString lesson;
	QString seat;
	QString student;
	int delta = 0;
	QString label;
	QString author;
};


QVector<DetailRow> detailRows( const ClassroomScoreBook& book )
{
	QMap<QString, QString> lessonTitles;
	for( const auto& lesson : book.lessons() )
	{
		lessonTitles.insert( lesson.id, lessonHeader( lesson ) );
	}

	QVector<DetailRow> rows;
	for( const auto& entry : book.entries() )
	{
		rows.append( DetailRow{ entry.timestamp, lessonTitles.value( entry.lessonId ),
								entry.seat, entry.student, entry.delta, entry.label, entry.author } );
	}
	std::stable_sort( rows.begin(), rows.end(), []( const DetailRow& a, const DetailRow& b ) {
		return a.timestamp < b.timestamp;
	} );
	return rows;
}


QString detailsWorksheet( const QVector<DetailRow>& rows )
{
	const QStringList headers{
		QCoreApplication::translate( "ClassroomTermReport", "Time" ),
		QCoreApplication::translate( "ClassroomTermReport", "Lesson" ),
		QCoreApplication::translate( "ClassroomTermReport", "Seat" ),
		QCoreApplication::translate( "ClassroomTermReport", "Name" ),
		QCoreApplication::translate( "ClassroomTermReport", "Delta" ),
		QCoreApplication::translate( "ClassroomTermReport", "Reason" ),
		QCoreApplication::translate( "ClassroomTermReport", "Operator" )
	};

	QString xml = QStringLiteral("<Worksheet ss:Name=\"%1\"><Table>\n")
					  .arg( sheetName( QCoreApplication::translate( "ClassroomTermReport", "Score details" ) ) );
	xml += QStringLiteral("<Row ss:Height=\"26\">");
	for( const auto& header : headers )
	{
		xml += QStringLiteral("<Cell ss:StyleID=\"head\"><Data ss:Type=\"String\">%1</Data></Cell>")
				   .arg( cellText( header ) );
	}
	xml += QStringLiteral("</Row>\n");

	for( const auto& row : rows )
	{
		xml += QStringLiteral("<Row>");
		xml += QStringLiteral("<Cell ss:StyleID=\"cell\"><Data ss:Type=\"String\">%1</Data></Cell>")
				   .arg( cellText( row.timestamp.toString( QStringLiteral("yyyy-MM-dd HH:mm") ) ) );
		xml += QStringLiteral("<Cell ss:StyleID=\"cell\"><Data ss:Type=\"String\">%1</Data></Cell>")
				   .arg( cellText( row.lesson ) );
		xml += QStringLiteral("<Cell ss:StyleID=\"cell\"><Data ss:Type=\"String\">%1</Data></Cell>")
				   .arg( cellText( row.seat ) );
		xml += QStringLiteral("<Cell ss:StyleID=\"cell\"><Data ss:Type=\"String\">%1</Data></Cell>")
				   .arg( cellText( row.student ) );
		xml += QStringLiteral("<Cell ss:StyleID=\"%1\"><Data ss:Type=\"Number\">%2</Data></Cell>")
				   .arg( row.delta > 0 ? QStringLiteral("good") :
						 ( row.delta < 0 ? QStringLiteral("bad") : QStringLiteral("num") ) )
				   .arg( row.delta );
		xml += QStringLiteral("<Cell ss:StyleID=\"cell\"><Data ss:Type=\"String\">%1</Data></Cell>")
				   .arg( cellText( row.label ) );
		xml += QStringLiteral("<Cell ss:StyleID=\"cell\"><Data ss:Type=\"String\">%1</Data></Cell>")
				   .arg( cellText( row.author ) );
		xml += QStringLiteral("</Row>\n");
	}
	xml += QStringLiteral("</Table></Worksheet>\n");
	return xml;
}
}


QByteArray ClassroomTermReport::toSpreadsheetMl( const ClassroomRoster& roster,
												 const ClassroomScoreBook& book,
												 const Options& options )
{
	const auto* classItem = roster.findClass( options.className );
	const auto lessons = filteredLessons( book, options );
	const int perLesson = options.attendanceColumn ? 3 : 2;
	const int columnCount = 2 + lessons.size() * perLesson + 1;

	const QString title = options.term.isEmpty() ?
							  QStringLiteral("%1 %2").arg( options.className, trText("Classroom score report") ) :
							  QStringLiteral("%1 %2 %3").arg( options.className, options.term,
															 trText("Classroom score report") );

	QString xml;
	xml += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	xml += QStringLiteral("<?mso-application progid=\"Excel.Sheet\"?>\n");
	xml += QStringLiteral("<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\" "
						  "xmlns:o=\"urn:schemas-microsoft-com:office:office\" "
						  "xmlns:x=\"urn:schemas-microsoft-com:office:excel\" "
						  "xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\" "
						  "xmlns:html=\"http://www.w3.org/TR/REC-html40\">\n");

	xml += QStringLiteral("<Styles>\n");
	xml += QStringLiteral("<Style ss:ID=\"Default\" ss:Name=\"Normal\"><Alignment ss:Vertical=\"Center\"/>"
						  "<Font ss:FontName=\"Microsoft YaHei\" ss:Size=\"11\"/></Style>\n");
	xml += QStringLiteral("<Style ss:ID=\"title\"><Font ss:FontName=\"Microsoft YaHei\" ss:Size=\"16\" ss:Bold=\"1\"/>"
						  "<Alignment ss:Horizontal=\"Center\" ss:Vertical=\"Center\"/></Style>\n");
	xml += QStringLiteral("<Style ss:ID=\"head\"><Font ss:FontName=\"Microsoft YaHei\" ss:Size=\"10\" ss:Bold=\"1\"/>"
						  "<Interior ss:Color=\"#DCE6F1\" ss:Pattern=\"Solid\"/>"
						  "<Alignment ss:Horizontal=\"Center\" ss:Vertical=\"Center\" ss:WrapText=\"1\"/>"
						  "<Borders><Border ss:Position=\"Bottom\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Top\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Left\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Right\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/></Borders></Style>\n");
	xml += QStringLiteral("<Style ss:ID=\"cell\"><Font ss:FontName=\"Microsoft YaHei\" ss:Size=\"10\"/>"
						  "<Alignment ss:Horizontal=\"Left\" ss:Vertical=\"Center\" ss:WrapText=\"1\"/>"
						  "<Borders><Border ss:Position=\"Bottom\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Top\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Left\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Right\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/></Borders></Style>\n");
	xml += QStringLiteral("<Style ss:ID=\"good\"><Font ss:FontName=\"Microsoft YaHei\" ss:Size=\"10\" "
						  "ss:Color=\"#008000\"/><Alignment ss:Horizontal=\"Left\" ss:Vertical=\"Center\" "
						  "ss:WrapText=\"1\"/>"
						  "<Borders><Border ss:Position=\"Bottom\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Top\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Left\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Right\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/></Borders></Style>\n");
	xml += QStringLiteral("<Style ss:ID=\"bad\"><Font ss:FontName=\"Microsoft YaHei\" ss:Size=\"10\" "
						  "ss:Color=\"#C00000\"/><Alignment ss:Horizontal=\"Left\" ss:Vertical=\"Center\" "
						  "ss:WrapText=\"1\"/>"
						  "<Borders><Border ss:Position=\"Bottom\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Top\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Left\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Right\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/></Borders></Style>\n");
	xml += QStringLiteral("<Style ss:ID=\"num\"><Font ss:FontName=\"Microsoft YaHei\" ss:Size=\"10\"/>"
						  "<Alignment ss:Horizontal=\"Center\" ss:Vertical=\"Center\"/>"
						  "<Borders><Border ss:Position=\"Bottom\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Top\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Left\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/>"
						  "<Border ss:Position=\"Right\" ss:LineStyle=\"Continuous\" ss:Weight=\"1\"/></Borders></Style>\n");
	xml += QStringLiteral("</Styles>\n");

	xml += QStringLiteral("<Worksheet ss:Name=\"%1\">\n").arg( sheetName( options.className ) );
	xml += QStringLiteral("<Table ss:DefaultColumnWidth=\"80\" ss:DefaultRowHeight=\"22\">\n");
	xml += QStringLiteral("<Column ss:Width=\"42\"/><Column ss:Width=\"90\"/>\n");
	for( int index = 0; index < lessons.size(); ++index )
	{
		xml += QStringLiteral("<Column ss:Width=\"140\"/>");
		if( options.attendanceColumn ) { xml += QStringLiteral("<Column ss:Width=\"56\"/>"); }
		xml += QStringLiteral("<Column ss:Width=\"80\"/>\n");
	}
	xml += QStringLiteral("<Column ss:Width=\"100\"/>\n");

	xml += QStringLiteral("<Row ss:Height=\"34\"><Cell ss:MergeAcross=\"%1\" ss:StyleID=\"title\">"
						  "<Data ss:Type=\"String\">%2</Data></Cell></Row>\n")
				.arg( columnCount - 1 ).arg( cellText( title ) );

	xml += QStringLiteral("<Row ss:Height=\"32\">");
	xml += QStringLiteral("<Cell ss:StyleID=\"head\"><Data ss:Type=\"String\">%1</Data></Cell>").arg( trText("No.") );
	xml += QStringLiteral("<Cell ss:StyleID=\"head\"><Data ss:Type=\"String\">%1</Data></Cell>").arg( trText("Name") );
	for( const auto& lesson : lessons )
	{
		xml += QStringLiteral("<Cell ss:StyleID=\"head\"><Data ss:Type=\"String\">%1</Data></Cell>")
					.arg( cellText( lessonHeader( lesson ) ) );
		if( options.attendanceColumn )
		{
			xml += QStringLiteral("<Cell ss:StyleID=\"head\"><Data ss:Type=\"String\">%1</Data></Cell>")
						.arg( trText("Attendance") );
		}
		xml += QStringLiteral("<Cell ss:StyleID=\"head\"><Data ss:Type=\"String\">%1</Data></Cell>")
					.arg( trText("Current total") );
	}
	xml += QStringLiteral("<Cell ss:StyleID=\"head\"><Data ss:Type=\"String\">%1</Data></Cell>")
				.arg( trText("Final total") );
	xml += QStringLiteral("</Row>\n");

	if( classItem != nullptr )
	{
		int row = 1;
		for( const auto& student : classItem->students )
		{
			xml += QStringLiteral("<Row ss:Height=\"46\">");
			xml += QStringLiteral("<Cell ss:StyleID=\"num\"><Data ss:Type=\"Number\">%1</Data></Cell>").arg( row++ );
			xml += QStringLiteral("<Cell ss:StyleID=\"cell\"><Data ss:Type=\"String\">%1</Data></Cell>")
						.arg( cellText( student.name ) );

			for( const auto& lesson : lessons )
			{
				const auto content = cellContent( book, lesson.id, student.seat );
				const auto delta = cellDelta( book, lesson.id, student.seat );
				const QString style = delta > 0 ? QStringLiteral("good") :
									  ( delta < 0 ? QStringLiteral("bad") : QStringLiteral("cell") );
				xml += QStringLiteral("<Cell ss:StyleID=\"%1\"><Data ss:Type=\"String\">%2</Data></Cell>")
							.arg( style, cellText( content ) );
				if( options.attendanceColumn )
				{
					xml += QStringLiteral("<Cell ss:StyleID=\"good\"><Data ss:Type=\"String\">%1</Data></Cell>")
								.arg( attendanceMark( book, lesson.id, student.seat ) );
				}
				xml += QStringLiteral("<Cell ss:StyleID=\"num\"><Data ss:Type=\"Number\">%1</Data></Cell>")
							.arg( book.total( student.seat, lesson.id ) );
			}
			xml += QStringLiteral("<Cell ss:StyleID=\"num\"><Data ss:Type=\"Number\">%1</Data></Cell>")
						.arg( book.total( student.seat ) );
			xml += QStringLiteral("</Row>\n");
		}
	}

	xml += QStringLiteral("</Table>\n");
	xml += QStringLiteral("<WorksheetOptions xmlns=\"urn:schemas-microsoft-com:office:excel\">"
						  "<FreezePanes/><FrozenNoSplit/><SplitHorizontal>2</SplitHorizontal>"
						  "<TopRowBottomPane>2</TopRowBottomPane><ActivePane>2</ActivePane>"
						  "</WorksheetOptions>\n");
	xml += QStringLiteral("</Worksheet>\n");

	if( options.includeDetails )
	{
		xml += detailsWorksheet( detailRows( book ) );
	}

	if( options.includeRules && book.rules().published )
	{
		QString plainRules = book.rules().html;
		plainRules.replace( QRegularExpression( QStringLiteral("<br\\s*/?>"),
												 QRegularExpression::CaseInsensitiveOption ), QStringLiteral("\n") );
		plainRules.replace( QRegularExpression( QStringLiteral("</p\\s*>"),
												 QRegularExpression::CaseInsensitiveOption ), QStringLiteral("\n") );
		plainRules.remove( QRegularExpression( QStringLiteral("<[^>]+>") ) );
		xml += QStringLiteral("<Worksheet ss:Name=\"%1\"><Table><Row><Cell><Data ss:Type=\"String\">%2"
							  "</Data></Cell></Row></Table></Worksheet>\n")
					.arg( sheetName( trText("Rules") ), cellText( plainRules.trimmed() ) );
	}

	xml += QStringLiteral("</Workbook>\n");
	return xml.toUtf8();
}


QByteArray ClassroomTermReport::toCsv( const ClassroomRoster& roster,
									   const ClassroomScoreBook& book,
									   const Options& options )
{
	const auto* classItem = roster.findClass( options.className );
	const auto lessons = filteredLessons( book, options );

	const auto quote = []( const QString& value ) {
		QString escaped = value;
		escaped.replace( QStringLiteral("\""), QStringLiteral("\"\"") );
		return QStringLiteral("\"%1\"").arg( escaped );
	};

	QString csv;
	csv += quote( trText("No.") ) + QLatin1Char(',') + quote( trText("Name") );
	for( const auto& lesson : lessons )
	{
		csv += QLatin1Char(',') + quote( lessonHeader( lesson ) );
		if( options.attendanceColumn ) { csv += QLatin1Char(',') + quote( trText("Attendance") ); }
		csv += QLatin1Char(',') + quote( trText("Current total") );
	}
	csv += QLatin1Char(',') + quote( trText("Final total") ) + QLatin1Char('\n');

	if( classItem != nullptr )
	{
		int row = 1;
		for( const auto& student : classItem->students )
		{
			csv += QString::number( row++ ) + QLatin1Char(',') + quote( student.name );
			for( const auto& lesson : lessons )
			{
				csv += QLatin1Char(',') + quote( cellContent( book, lesson.id, student.seat ) );
				if( options.attendanceColumn )
				{
					csv += QLatin1Char(',') + quote( attendanceMark( book, lesson.id, student.seat ) );
				}
				csv += QLatin1Char(',') + QString::number( book.total( student.seat, lesson.id ) );
			}
			csv += QLatin1Char(',') + QString::number( book.total( student.seat ) ) + QLatin1Char('\n');
		}
	}

	QByteArray data( "\xEF\xBB\xBF", 3 );
	data.append( csv.toUtf8() );
	return data;
}


QByteArray ClassroomTermReport::toDetailsCsv( const ClassroomScoreBook& book, const Options& options )
{
	const auto quote = []( const QString& value ) {
		QString escaped = value;
		escaped.replace( QStringLiteral("\""), QStringLiteral("\"\"") );
		return QStringLiteral("\"%1\"").arg( escaped );
	};

	QString csv;
	csv += quote( trText("Class") ) + QLatin1Char(',') + quote( options.className ) + QLatin1Char('\n');

	const QStringList headers{
		trText("Time"), trText("Lesson"), trText("Seat"), trText("Name"),
		trText("Delta"), trText("Reason"), trText("Operator")
	};
	for( int index = 0; index < headers.size(); ++index )
	{
		if( index > 0 ) { csv += QLatin1Char(','); }
		csv += quote( headers.at( index ) );
	}
	csv += QLatin1Char('\n');

	for( const auto& row : detailRows( book ) )
	{
		csv += quote( row.timestamp.toString( QStringLiteral("yyyy-MM-dd HH:mm") ) ) + QLatin1Char(',') +
			   quote( row.lesson ) + QLatin1Char(',') +
			   quote( row.seat ) + QLatin1Char(',') +
			   quote( row.student ) + QLatin1Char(',') +
			   QString::number( row.delta ) + QLatin1Char(',') +
			   quote( row.label ) + QLatin1Char(',') +
			   quote( row.author ) + QLatin1Char('\n');
	}

	QByteArray data( "\xEF\xBB\xBF", 3 );
	data.append( csv.toUtf8() );
	return data;
}


bool ClassroomTermReport::exportXls( const QString& fileName, const ClassroomRoster& roster,
									 const ClassroomScoreBook& book, const Options& options,
									 QString* error )
{
	if( QDir().mkpath( QFileInfo( fileName ).absolutePath() ) == false )
	{
		if( error ) { *error = trText("Could not create the destination directory."); }
		return false;
	}
	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		if( error ) { *error = file.errorString(); }
		return false;
	}
	const auto data = toSpreadsheetMl( roster, book, options );
	if( file.write( data ) != data.size() )
	{
		if( error ) { *error = file.errorString(); }
		return false;
	}
	return true;
}
