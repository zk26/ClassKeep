/*
 * ClassroomSecrets.h - protect secrets on disk and authenticate bridge requests
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include <QByteArray>
#include <QString>

class ClassroomSecrets
{
public:
	//! Windows: CryptProtectData bound to the interactive teacher account.
	//! The "dpapi:" prefix is what is written to settings.json.
	static QString protect( const QString& plain );
	static QString reveal( const QString& stored );

	//! True for the pre-2.3 "enc:" format (encrypted with the shipped public key).
	static bool isLegacyProtected( const QString& stored );

	//! 128 bit of CryptGenRandom-backed entropy, hex encoded (32 characters).
	static QString generateBridgeKey();
	//! One-time challenge for a single HTTP request, hex encoded.
	static QString generateNonce();

	//! HMAC-SHA256(key, method "\n" path "\n" nonce "\n" SHA256(body)), lowercase hex.
	//! Binding method/path/body makes a captured token useless for any other request.
	static QByteArray requestToken( const QString& key, const QString& method, const QString& path,
									const QString& nonce, const QByteArray& body );

	static bool constantTimeEquals( const QByteArray& left, const QByteArray& right );
};
