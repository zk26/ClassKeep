/*
 * ClassroomSecrets.cpp - store connection keys and API keys encrypted on disk
 *
 * This file is part of the Veyon classroom modification.
 */

#include "ClassroomSecrets.h"

#include <QCryptographicHash>

#include "CryptoCore.h"
#include "VeyonCore.h"


QString ClassroomSecrets::protect( const QString& plain )
{
	const QString prefix = QStringLiteral("enc:");
	if( plain.isEmpty() || plain.startsWith( prefix ) ) { return plain; }

	const auto encrypted = VeyonCore::cryptoCore().
						   encryptPassword( CryptoCore::PlaintextPassword( plain.toUtf8() ) );
	return prefix + encrypted;
}


QString ClassroomSecrets::reveal( const QString& stored )
{
	const QString prefix = QStringLiteral("enc:");
	if( stored.startsWith( prefix ) == false ) { return stored; }

	const auto decrypted = VeyonCore::cryptoCore().decryptPassword( stored.mid( prefix.size() ) );
	return QString::fromUtf8( decrypted.toByteArray() );
}


QString ClassroomSecrets::sessionToken( const QString& lessonKey, const QString& nonce )
{
	const QByteArray data = QStringLiteral("veyon-classroom:%1:%2").arg( lessonKey, nonce ).toUtf8();
	return QString::fromLatin1( QCryptographicHash::hash( data, QCryptographicHash::Sha256 ).toHex() );
}
