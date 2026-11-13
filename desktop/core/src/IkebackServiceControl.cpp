/*
 * IkebackServiceControl.cpp - class for controlling Ikeback service
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

#include <QCoreApplication>
#include <QDir>

#include "PlatformServiceFunctions.h"
#include "IkebackServiceControl.h"


IkebackServiceControl::IkebackServiceControl( QWidget* parent ) :
	ServiceControl( name(), filePath(), displayName(), parent )
{
}



bool IkebackServiceControl::setAutostart( bool enabled )
{
	return IkebackCore::platform().serviceFunctions().
			setStartMode( name(), enabled ? PlatformServiceFunctions::StartMode::Auto :
											PlatformServiceFunctions::StartMode::Manual );
}



QString IkebackServiceControl::name()
{
	return IkebackCore::platform().serviceFunctions().ikebackServiceName();
}



QString IkebackServiceControl::filePath()
{
	return QDir::toNativeSeparators(
				QCoreApplication::applicationDirPath() +
				QDir::separator() +
				QStringLiteral("ikeback-service") + IkebackCore::executableSuffix() );
}



QString IkebackServiceControl::displayName()
{
	return tr( "ikeback Service" );
}
