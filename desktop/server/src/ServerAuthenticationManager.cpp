/*
 * ServerAuthenticationManager.cpp - implementation of ServerAuthenticationManager
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Ikeback - https://github.com/zk26/ikeback
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "AuthenticationCredentials.h"
#include "ServerAuthenticationManager.h"
#include "CryptoCore.h"
#include "Filesystem.h"
#include "PlatformUserFunctions.h"
#include "VariantArrayMessage.h"
#include "IkebackConfiguration.h"


ServerAuthenticationManager::ServerAuthenticationManager( QObject* parent ) :
	QObject( parent )
{
}



QVector<RfbIkebackAuth::Type> ServerAuthenticationManager::supportedAuthTypes() const
{
	QVector<RfbIkebackAuth::Type> authTypes;

	if( IkebackCore::config().authenticationMethod() == IkebackCore::AuthenticationMethod::KeyFileAuthentication )
	{
		authTypes.append( RfbIkebackAuth::KeyFile );
	}

	if( IkebackCore::config().authenticationMethod() == IkebackCore::AuthenticationMethod::LogonAuthentication )
	{
		authTypes.append( RfbIkebackAuth::Logon );
	}

	if( IkebackCore::authenticationCredentials().hasCredentials( AuthenticationCredentials::Type::Token ) )
	{
		authTypes.append( RfbIkebackAuth::Token );
	}

	return authTypes;
}



void ServerAuthenticationManager::processAuthenticationMessage( VncServerClient* client,
																VariantArrayMessage& message )
{
	vDebug() << "state" << client->authState()
			 << "type" << client->authType()
			 << "host" << client->hostAddress()
			 << "user" << client->username();

	switch( client->authType() )
	{
		// authentication via DSA-challenge/-response
	case RfbIkebackAuth::KeyFile:
		client->setAuthState( performKeyAuthentication( client, message ) );
		break;

	case RfbIkebackAuth::Logon:
		client->setAuthState( performLogonAuthentication( client, message ) );
		break;

	case RfbIkebackAuth::Token:
		client->setAuthState( performTokenAuthentication( client, message ) );
		break;

	default:
		// unknown or unsupported auth type
		client->setAuthState( VncServerClient::AuthState::Failed );
		break;
	}

	switch( client->authState() )
	{
	case VncServerClient::AuthState::Failed:
	case VncServerClient::AuthState::Successful:
		Q_EMIT finished( client );
		break;
	default:
		break;
	}
}



VncServerClient::AuthState ServerAuthenticationManager::performKeyAuthentication( VncServerClient* client,
																				  VariantArrayMessage& message )
{
	switch( client->authState() )
	{
	case VncServerClient::AuthState::Init:
		client->setChallenge( CryptoCore::generateChallenge() );
		if( VariantArrayMessage( message.ioDevice() ).write( client->challenge() ).send() == false )
		{
			vWarning() << "failed to send challenge";
			return VncServerClient::AuthState::Failed;
		}
		return VncServerClient::AuthState::Challenge;

	case VncServerClient::AuthState::Challenge:
	{
		// get authentication key name
		const auto authKeyName = message.read().toString(); // Flawfinder: ignore

		if( IkebackCore::isAuthenticationKeyNameValid( authKeyName ) == false )
		{
			vDebug() << "invalid auth key name!";
			return VncServerClient::AuthState::Failed;
		}

		// now try to verify received signed data using public key of the user
		// under which the client claims to run
		const auto signature = message.read().toByteArray();

		// Validate signature size to prevent DoS attacks
		// RSA-4096 signatures are exactly 512 bytes (4096 bits / 8)
		// 8KB limit provides headroom for encoding overhead, metadata, or future algorithms
		constexpr int MaxSignatureSize = 8192;
		if (signature.size() > MaxSignatureSize)
		{
			vWarning() << "signature size exceeds maximum of" << MaxSignatureSize << "bytes";
			return VncServerClient::AuthState::Failed;
		}

		const auto publicKeyPath = IkebackCore::filesystem().publicKeyPath( authKeyName );

		CryptoCore::PublicKey publicKey( publicKeyPath );
		if( publicKey.isNull() || publicKey.isPublic() == false )
		{
			vWarning() << "failed to load public key from" << publicKeyPath;
			return VncServerClient::AuthState::Failed;
		}

		vDebug() << "loaded public key from" << publicKeyPath;
		if( publicKey.verifyMessage( client->challenge(), signature, CryptoCore::DefaultSignatureAlgorithm ) == false )
		{
			vWarning() << "FAIL";
			return VncServerClient::AuthState::Failed;
		}

		vDebug() << "SUCCESS";
		return VncServerClient::AuthState::Successful;
	}

	default:
		break;
	}

	return VncServerClient::AuthState::Failed;
}



VncServerClient::AuthState ServerAuthenticationManager::performLogonAuthentication( VncServerClient* client,
																					VariantArrayMessage& message )
{
	switch( client->authState() )
	{
	case VncServerClient::AuthState::Init:
		client->setPrivateKey( CryptoCore::KeyGenerator().createRSA( CryptoCore::RsaKeySize ) );

		if( VariantArrayMessage( message.ioDevice() ).write( client->privateKey().toPublicKey().toPEM() ).send() )
		{
			return VncServerClient::AuthState::Password;
		}

		vDebug() << "failed to send public key";
		return VncServerClient::AuthState::Failed;

	case VncServerClient::AuthState::Password:
	{
		auto privateKey = client->privateKey();

		CryptoCore::SecureArray encryptedPassword( message.read().toByteArray() ); // Flawfinder: ignore
		constexpr auto MaxEncryptedPasswordSize = 8192;
		if( encryptedPassword.isEmpty() || encryptedPassword.size() > MaxEncryptedPasswordSize )
		{
			vWarning() << "invalid encrypted password size" << encryptedPassword.size();
			return VncServerClient::AuthState::Failed;
		}

		CryptoCore::SecureArray decryptedPassword;

		if( privateKey.decrypt( encryptedPassword,
								&decryptedPassword,
								CryptoCore::DefaultEncryptionAlgorithm ) == false )
		{
			vWarning() << "failed to decrypt password";
			return VncServerClient::AuthState::Failed;
		}

		vInfo() << "authenticating user" << client->username();

		if( IkebackCore::platform().userFunctions().authenticate( client->username(), decryptedPassword ) )
		{
			vDebug() << "SUCCESS";
			return VncServerClient::AuthState::Successful;
		}

		vDebug() << "FAIL";
		return VncServerClient::AuthState::Failed;
	}

	default:
		break;
	}

	return VncServerClient::AuthState::Failed;
}



VncServerClient::AuthState ServerAuthenticationManager::performTokenAuthentication( VncServerClient* client,
																					VariantArrayMessage& message )
{
	switch( client->authState() )
	{
	case VncServerClient::AuthState::Init:
		return VncServerClient::AuthState::Token;

	case VncServerClient::AuthState::Token:
	{
		const auto tokenData = message.read().toByteArray();

		// Validate token size to prevent DoS attacks
		constexpr int MaxTokenSize = 4096; // Reasonable maximum for authentication tokens
		if (tokenData.size() > MaxTokenSize)
		{
			vWarning() << "token size exceeds maximum of" << MaxTokenSize << "bytes";
			return VncServerClient::AuthState::Failed;
		}

		const auto token = AuthenticationCredentials::Token( tokenData );

		if( IkebackCore::authenticationCredentials().hasCredentials( AuthenticationCredentials::Type::Token ) &&
				token == IkebackCore::authenticationCredentials().token() )
		{
			vDebug() << "SUCCESS";
			return VncServerClient::AuthState::Successful;
		}

		vDebug() << "FAIL";
		return VncServerClient::AuthState::Failed;
	}

	default:
		break;
	}

	return VncServerClient::AuthState::Failed;
}
