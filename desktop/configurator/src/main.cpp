/*
 * main.cpp - main file for Ikeback Configurator
 *
 * Copyright (c) 2010-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QApplication>
#include <QMessageBox>

#include "IkebackConfiguration.h"
#include "IkebackCore.h"
#include "MainWindow.h"
#include "PlatformCoreFunctions.h"
#include "Logger.h"



int main( int argc, char **argv )
{
	IkebackCore::setupApplicationParameters();

	QApplication app( argc, argv );

	IkebackCore core( &app, IkebackCore::Component::Configurator, QStringLiteral("Configurator") );

	// make sure to run as admin
	if( qEnvironmentVariableIntValue( "IKEBACK_CONFIGURATOR_NO_ELEVATION" ) == 0 &&
		IkebackCore::platform().coreFunctions().isRunningAsAdmin() == false &&
		app.arguments().size() <= 1 )
	{
		if( IkebackCore::platform().coreFunctions().runProgramAsAdmin( QCoreApplication::applicationFilePath(), {
																	 QStringLiteral("-elevated") } ) )
		{
			return 0;
		}

		QMessageBox::warning( nullptr, MainWindow::tr( "Insufficient privileges" ),
							  MainWindow::tr( "Could not start with administrative privileges. "
											  "Please make sure a sudo-like program is installed "
											  "for your desktop environment! The program will "
											  "be run with normal user privileges.") );
	}

	if( IkebackConfiguration().isStoreWritable() == false &&
		IkebackCore::config().logLevel() != Logger::LogLevel::Debug )
	{
		QMessageBox::critical(nullptr,
							  MainWindow::tr("Configuration not writable"),
							  MainWindow::tr("The local configuration backend reported that the "
											 "configuration is not writable! Please run ikeback "
											 "Configurator with higher privileges."));
		return -1;
	}

	// now create the main window
	auto mainWindow = new MainWindow;
	mainWindow->show();

	return core.exec();
}
