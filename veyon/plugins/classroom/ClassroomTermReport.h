/*
 * ClassroomTermReport.h - end of term class report matching the teacher's spreadsheet
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QByteArray>
#include <QDate>
#include <QString>

class ClassroomRoster;
class ClassroomScoreBook;

class ClassroomTermReport
{
public:
	struct Options
	{
		QString className;
		QString term;
		QDate from;
		QDate to;
		bool includeRules = false;
		bool attendanceColumn = true;
	};

	static QByteArray toSpreadsheetMl( const ClassroomRoster& roster,
									   const ClassroomScoreBook& book,
									   const Options& options );
	static QByteArray toCsv( const ClassroomRoster& roster,
							 const ClassroomScoreBook& book,
							 const Options& options );

	static bool exportXls( const QString& fileName, const ClassroomRoster& roster,
						   const ClassroomScoreBook& book, const Options& options,
						   QString* error = nullptr );
};
