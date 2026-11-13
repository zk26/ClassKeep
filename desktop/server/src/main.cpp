/*
 * main.cpp - main file for Ikeback Server
 *
 * Copyright (c) 2006-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QGuiApplication>

#include "ComputerControlServer.h"


int main( int argc, char **argv )
{
	IkebackCore::setupApplicationParameters();

	QGuiApplication app( argc, argv );
	QGuiApplication::setDesktopFileName(QStringLiteral("io.ikeback.ikeback-server"));

	IkebackCore core( &app, IkebackCore::Component::Server, QStringLiteral("Server") );

	ComputerControlServer server( &core );
	if( server.start() == false )
	{
		vCritical() << "Failed to start server";
		return -1;
	}

	return core.exec();
}
