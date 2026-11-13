/*
 * ConfigurationManager.cpp - class for managing Ikeback's configuration
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

#include "Configuration/LocalStore.h"
#include "ConfigurationManager.h"
#include "Filesystem.h"
#include "PlatformCoreFunctions.h"
#include "PlatformNetworkFunctions.h"
#include "IkebackConfiguration.h"
#include "IkebackServiceControl.h"



ConfigurationManager::ConfigurationManager( QObject* parent ) :
	QObject( parent ),
	m_configuration( IkebackCore::config() )
{
}



bool ConfigurationManager::clearConfiguration()
{
	Configuration::LocalStore(Configuration::LocalStore::Scope::System).clear();

	return true;
}



bool ConfigurationManager::applyConfiguration()
{
	// update Ikeback Service configuration
	if( IkebackServiceControl().setAutostart( m_configuration.autostartService() ) == false )
	{
		m_errorString = tr("Could not modify the autostart property for the Ikeback Service.");
		return false;
	}

	auto& network = IkebackCore::platform().networkFunctions();

	if( network.configureFirewallException( IkebackCore::filesystem().serverFilePath(),
											QStringLiteral("ikeback Server"),
											m_configuration.isFirewallExceptionEnabled() ) == false )
	{
		m_errorString = tr("Could not configure the firewall configuration for the Ikeback Server.");
		return false;
	}

	if( network.configureFirewallException( IkebackCore::filesystem().workerFilePath(),
											QStringLiteral("ikeback Worker"),
											m_configuration.isFirewallExceptionEnabled() ) == false )
	{
		m_errorString = tr("Could not configure the firewall configuration for the Ikeback Worker.");
		return false;
	}

	if( IkebackCore::platform().coreFunctions().applyConfiguration() == false )
	{
		m_errorString =  tr("Could not apply platform-specific configuration settings.");
		return false;
	}

	return true;
}



bool ConfigurationManager::saveConfiguration()
{
	// write global configuration
	Configuration::LocalStore localStore(Configuration::LocalStore::Scope::System);
	if( localStore.isWritable() == false )
	{
		m_errorString = tr( "Configuration is not writable. Please check your permissions!" );
		return false;
	}

	localStore.flush( &m_configuration );
	return true;
}
