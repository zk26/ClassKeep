/*
 * IkebackConfiguration.cpp - a Configuration object storing system wide
 *                          configuration values
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

#include <QDir>

#include "IkebackConfiguration.h"
#include "IkebackCore.h"
#include "Logger.h"
#include "NetworkObjectDirectory.h"


IkebackConfiguration::IkebackConfiguration() :
	Configuration::Object(Configuration::Store::Backend::Local,
						  Configuration::Store::Scope::System)
{
}



IkebackConfiguration::IkebackConfiguration( Configuration::Store* store ) :
	Configuration::Object( store )
{
}



void IkebackConfiguration::upgrade()
{
	if( applicationVersion() < IkebackCore::ApplicationVersion::Version_4_2 )
	{
		setAutoSelectCurrentLocation( legacyAutoSwitchToCurrentRoom() );
		setShowCurrentLocationOnly( legacyOnlyCurrentRoomVisible() );
		setAllowAddingHiddenLocations( legacyManualRoomAdditionAllowed() );
		setHideEmptyLocations( legacyEmptyRoomsHidden() );
		setAutoOpenComputerSelectPanel( legacyOpenComputerManagementAtStart() );
		setConfirmUnsafeActions( legacyConfirmDangerousActions() );

		setApplicationVersion( IkebackCore::ApplicationVersion::Version_4_2 );
	}
	if( applicationVersion() < IkebackCore::ApplicationVersion::Version_4_5 )
	{
		setIkebackServerPort( legacyPrimaryServicePort() );
		setHideLocalComputer( legacyLocalComputerHidden() );
		setHideComputerFilter( legacyComputerFilterHidden() );
		setAutoAdjustMonitoringIconSize( legacyAutoAdjustGridSize() );

		setApplicationVersion( IkebackCore::ApplicationVersion::Version_4_5 );
	}
	if (applicationVersion() < IkebackCore::ApplicationVersion::Version_4_7)
	{
		setApplicationVersion(IkebackCore::ApplicationVersion::Version_4_7);
	}
	if (applicationVersion() < IkebackCore::ApplicationVersion::Version_4_8)
	{
		setApplicationVersion(IkebackCore::ApplicationVersion::Version_4_8);
	}
	if (applicationVersion() < IkebackCore::ApplicationVersion::Version_4_9)
	{
		setUserGroupsBackend(legacyAccessControlUserGroupsBackend());
		setUseDomainUserGroups(legacyDomainGroupsForAccessControlEnabled());
		setApplicationVersion(IkebackCore::ApplicationVersion::Version_4_9);
	}
	if (applicationVersion() < IkebackCore::ApplicationVersion::Version_4_10)
	{
		setApplicationVersion(IkebackCore::ApplicationVersion::Version_4_10);
	}
	if (applicationVersion() < IkebackCore::ApplicationVersion::Version_4_11)
	{
		setApplicationVersion(IkebackCore::ApplicationVersion::Version_4_11);
	}
}
