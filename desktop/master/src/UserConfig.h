/*
 * UserConfig.h - UserConfig class
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

#pragma once

#include <QJsonArray>
#include <QJsonObject>

#include "Configuration/Object.h"
#include "Configuration/Property.h"
#include "ComputerMonitoringView.h"
#include "SlideshowPanel.h"
#include "IkebackMaster.h"

// clazy:excludeall=ctor-missing-parent-argument,copyable-polymorphic

class UserConfig : public Configuration::Object
{
	Q_OBJECT
public:
	UserConfig(const QString& storeName);
	explicit UserConfig(Configuration::Store* store);

#define FOREACH_PERSONAL_CONFIG_PROPERTY(OP)						\
	OP( UserConfig, IkebackMaster::userConfig(), QJsonArray, checkedNetworkObjects, setCheckedNetworkObjects, "CheckedNetworkObjects", "UI", QJsonArray(), Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), QJsonArray, computerPositions, setComputerPositions, "ComputerPositions", "UI", QJsonArray(), Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), bool, useCustomComputerPositions, setUseCustomComputerPositions, "UseCustomComputerPositions", "UI", false, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), bool, filterPoweredOnComputers, setFilterPoweredOnComputers, "FilterPoweredOnComputers", "UI", false, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), bool, filterComputersWithLoggedOnUsers, setFilterComputersWithLoggedOnUsers, "FilterComputersWithLoggedOnUsers", "UI", false, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), int, monitoringScreenSize, setMonitoringScreenSize, "MonitoringScreenSize", "UI", ComputerMonitoringView::DefaultComputerScreenSize, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), bool, autoAdjustMonitoringIconSize, setAutoAdjustMonitoringIconSize, "AutoAdjustMonitoringIconSize", "UI", false, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), int, slideshowDuration, setSlideshowDuration, "SlideshowDuration", "UI", SlideshowPanel::DefaultDuration, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), bool, spotlightRealtime, setSpotlightRealtime, "SpotlightRealtime", "UI", true, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), int, defaultRole, setDefaultRole, "DefaultRole", "Authentication", 0, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), bool, toolButtonIconOnlyMode, setToolButtonIconOnlyMode, "ToolButtonIconOnlyMode", "UI", false, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), bool, noToolTips, setNoToolTips, "NoToolTips", "UI", false, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), QJsonObject, splitterStates, setSplitterStates, "SplitterStates", "UI", {}, Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), QString, windowState, setWindowState, "WindowState", "UI", QString(), Configuration::Property::Flag::Standard )	\
	OP( UserConfig, IkebackMaster::userConfig(), QString, windowGeometry, setWindowGeometry, "WindowGeometry", "UI", QString(), Configuration::Property::Flag::Standard )	\

	FOREACH_PERSONAL_CONFIG_PROPERTY(DECLARE_CONFIG_PROPERTY)

} ;
