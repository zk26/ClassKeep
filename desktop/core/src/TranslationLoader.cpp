/*
 * TranslationLoader.cpp - implementation of TranslationLoader class
 *
 * Copyright (c) 2020-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QCoreApplication>
#include <QRegularExpression>
#include <QTranslator>

#include "TranslationLoader.h"
#include "IkebackConfiguration.h"
#include "IkebackCore.h"


TranslationLoader::TranslationLoader( const QString& resourceName )
{
	load( resourceName );
}



bool TranslationLoader::load( const QString& resourceName )
{
	QLocale configuredLocale( QLocale::C );

	static const QRegularExpression configuredLocaleRX{QStringLiteral( "[^(]*\\(([^)]*)\\)")};
	const auto configuredLocaleMatch = configuredLocaleRX.match(IkebackCore::config().uiLanguage());
	if( configuredLocaleMatch.hasMatch() )
	{
		configuredLocale = QLocale( configuredLocaleMatch.captured( 1 ) );
	}
	// ikeback: a fresh installation must come up in Simplified Chinese.
	// An explicit choice in Configurator -> General -> Language still wins,
	// because the regex above overwrites configuredLocale in that case.
	// If zh_CN .qm files are absent, the existing system-locale fallback applies.
	else
	{
		configuredLocale = QLocale( QStringLiteral("zh_CN") );
	}

	if( configuredLocale.language() != QLocale::English &&
		IkebackCore::instance()->findChild<QTranslator *>( resourceName ) == nullptr )
	{
		const auto translationsDirectory = resourceName.startsWith( QLatin1String("qt") )
											  ? IkebackCore::qtTranslationsDirectory()
											  : IkebackCore::translationsDirectory();

		auto translator = new QTranslator( IkebackCore::instance() );
		translator->setObjectName( resourceName );

		if( configuredLocale == QLocale::C ||
			translator->load( QStringLiteral( "%1_%2.qm" ).arg( resourceName, configuredLocale.name() ),
							  translationsDirectory ) == false )
		{
			configuredLocale = QLocale::system(); // Flawfinder: ignore

			if( translator->load( QStringLiteral( "%1_%2.qm" ).arg( resourceName, configuredLocale.name() ),
								  translationsDirectory ) == false )
			{
				delete translator;
				return false;
			}
		}

		QLocale::setDefault( configuredLocale );

		QCoreApplication::installTranslator( translator );
	}

	return true;
}
