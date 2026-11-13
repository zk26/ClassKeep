/*
 * ClassroomSecrets.cpp - protect secrets on disk and authenticate bridge requests
 *
 * This file is part of the Veyon classroom modification.
 */

#include "ClassroomSecrets.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QDebug>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

#include "CryptoCore.h"
#include "VeyonCore.h"


static const QString dpapiPrefix = QStringLiteral("dpapi:");
static const QString legacyPrefix = QStringLiteral("enc:");


static QByteArray randomHex( int byteCount, bool upperCase )
{
	QByteArray result;
	result.reserve( byteCount * 2 );
	for( int index = 0; index < byteCount; ++index )
	{
		result += QByteArray::number( static_cast<uint>( QRandomGenerator::system()->generate() & 0xff ), 16 )
					  .rightJustified( 2, '0', false );
	}
	if( upperCase ) { result = result.toUpper(); }
	return result;
}


#ifdef Q_OS_WIN
static QByteArray dpapiProtect( const QByteArray& plain )
{
	DATA_BLOB input{};
	input.cbData = static_cast<DWORD>( plain.size() );
	input.pbData = reinterpret_cast<BYTE*>( const_cast<char*>( plain.constData() ) );
	DATA_BLOB output{};
	if( CryptProtectData( &input, L"ikeback", nullptr, nullptr, nullptr,
						  CRYPTPROTECT_UI_FORBIDDEN, &output ) == FALSE )
	{
		return {};
	}
	const QByteArray result( reinterpret_cast<const char*>( output.pbData ),
							 static_cast<int>( output.cbData ) );
	LocalFree( output.pbData );
	return result;
}


static QByteArray dpapiUnprotect( const QByteArray& blob )
{
	DATA_BLOB input{};
	input.cbData = static_cast<DWORD>( blob.size() );
	input.pbData = reinterpret_cast<BYTE*>( const_cast<char*>( blob.constData() ) );
	DATA_BLOB output{};
	if( CryptUnprotectData( &input, nullptr, nullptr, nullptr, nullptr,
							CRYPTPROTECT_UI_FORBIDDEN, &output ) == FALSE )
	{
		return {};
	}
	const QByteArray result( reinterpret_cast<const char*>( output.pbData ),
							 static_cast<int>( output.cbData ) );
	LocalFree( output.pbData );
	return result;
}
#endif


QString ClassroomSecrets::protect( const QString& plain )
{
	if( plain.isEmpty() || plain.startsWith( dpapiPrefix ) ) { return plain; }

#ifdef Q_OS_WIN
	const auto blob = dpapiProtect( plain.toUtf8() );
	if( blob.isEmpty() == false ) { return dpapiPrefix + QString::fromLatin1( blob.toBase64() ); }
	// Fail closed: never silently downgrade a secret to plaintext or to the
	// repository-wide RSA key just because DPAPI is unavailable.
	qWarning() << "ikeback: could not protect a secret with DPAPI";
	return {};
#else
	// ikeback's supported deployment target is Windows. Keep the legacy
	// format on other platforms so the tree still builds for CI/development;
	// ClassroomSettings::save() restricts the file to the owner there.
	const auto encrypted = VeyonCore::cryptoCore().
						   encryptPassword( CryptoCore::PlaintextPassword( plain.toUtf8() ) );
	return legacyPrefix + encrypted;
#endif
}


QString ClassroomSecrets::reveal( const QString& stored )
{
	if( stored.startsWith( dpapiPrefix ) )
	{
#ifdef Q_OS_WIN
		const auto blob = QByteArray::fromBase64( stored.mid( dpapiPrefix.size() ).toLatin1() );
		return QString::fromUtf8( dpapiUnprotect( blob ) );
#else
		qWarning() << "ikeback: dpapi: secret cannot be read on this platform";
		return {};
#endif
	}
	if( stored.startsWith( legacyPrefix ) )
	{
		const auto decrypted = VeyonCore::cryptoCore().decryptPassword( stored.mid( legacyPrefix.size() ) );
		return QString::fromUtf8( decrypted.toByteArray() );
	}
	// Pre-2.3 files stored these values in plain text.
	return stored;
}


bool ClassroomSecrets::isLegacyProtected( const QString& stored )
{
	return stored.startsWith( legacyPrefix );
}


QString ClassroomSecrets::generateBridgeKey()
{
	return QString::fromLatin1( randomHex( 16, true ) );
}


QString ClassroomSecrets::generateNonce()
{
	return QString::fromLatin1( randomHex( 16, false ) );
}


QByteArray ClassroomSecrets::requestToken( const QString& key, const QString& method, const QString& path,
										   const QString& nonce, const QByteArray& body )
{
	QByteArray message = method.toUtf8();
	message += '\n';
	message += path.toUtf8();
	message += '\n';
	message += nonce.toUtf8();
	message += '\n';
	message += QCryptographicHash::hash( body, QCryptographicHash::Sha256 ).toHex();
	return QMessageAuthenticationCode::hash( message, key.toUtf8(), QCryptographicHash::Sha256 ).toHex();
}


bool ClassroomSecrets::constantTimeEquals( const QByteArray& left, const QByteArray& right )
{
	if( left.size() != right.size() ) { return false; }
	unsigned char difference = 0;
	for( int index = 0; index < left.size(); ++index )
	{
		difference = static_cast<unsigned char>( difference |
			( static_cast<unsigned char>( left.at( index ) ) ^ static_cast<unsigned char>( right.at( index ) ) ) );
	}
	return difference == 0;
}
