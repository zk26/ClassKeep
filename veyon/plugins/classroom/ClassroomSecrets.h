/*
 * ClassroomSecrets.h - store connection keys and API keys encrypted on disk
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QString>

class ClassroomSecrets
{
public:
	static QString protect( const QString& plain );
	static QString reveal( const QString& stored );

	//! SHA-256("veyon-classroom:<lesson key>:<challenge>") shared by teacher bridge and mobile app.
	static QString sessionToken( const QString& lessonKey, const QString& nonce );
};
