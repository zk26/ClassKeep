/*
 * ClassroomSeatOrder.h - persistent custom seat ordering shared by the teacher
 *                        console, the seat grid and the mobile bridge.
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStringList>

namespace Classroom
{

/** One file per class, next to the rest of the ikeback data. */
inline QString seatOrderPath( const QString& className )
{
	const auto base = QStandardPaths::writableLocation( QStandardPaths::AppLocalDataLocation )
					  + QStringLiteral("/ikeback");
	QDir().mkpath( base );
	// Class names come from a CSV; keep only filesystem-safe characters.
	QString safe = className;
	safe.replace( QLatin1Char('/'), QLatin1Char('_') );
	safe.replace( QLatin1Char('\\'), QLatin1Char('_') );
	safe.replace( QLatin1Char(':'), QLatin1Char('_') );
	return base + QStringLiteral("/seat-order-") + safe + QStringLiteral(".json");
}


inline QStringList loadSeatOrder( const QString& className )
{
	if( className.isEmpty() ) { return {}; }

	QFile file( seatOrderPath( className ) );
	if( file.open( QFile::ReadOnly ) == false ) { return {}; }

	const auto document = QJsonDocument::fromJson( file.readAll() );
	if( document.isObject() == false ) { return {}; }

	QStringList order;
	const auto seats = document.object().value( QStringLiteral("seats") ).toArray();
	for( const auto& value : seats )
	{
		const auto seat = value.toString();
		if( seat.isEmpty() == false ) { order.append( seat ); }
	}
	return order;
}


inline void saveSeatOrder( const QString& className, const QStringList& order )
{
	if( className.isEmpty() ) { return; }

	QJsonArray seats;
	for( const auto& seat : order ) { seats.append( seat ); }

	QJsonObject root;
	root.insert( QStringLiteral("className"), className );
	root.insert( QStringLiteral("seats"), seats );

	QFile file( seatOrderPath( className ) );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( root ).toJson( QJsonDocument::Indented ) );
	}
}

} // namespace Classroom
