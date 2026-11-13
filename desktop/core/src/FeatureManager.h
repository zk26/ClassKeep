/*
 * FeatureManager.h - header for the FeatureManager class
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

#include <QObject>

#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "ComputerControlInterface.h"
#include "Plugin.h"

class QWidget;

class FeatureWorkerManager;
class IkebackServerInterface;

class IKEBACK_CORE_EXPORT FeatureManager : public QObject
{
	Q_OBJECT
public:
	explicit FeatureManager( QObject* parent = nullptr );

	const FeatureList& features() const
	{
		return m_features;
	}

	const FeatureList& features( Plugin::Uid pluginUid ) const;

	const Feature& feature( Feature::Uid featureUid ) const;

	const FeatureList& relatedFeatures( Feature::Uid featureUid ) const;

	Feature::Uid metaFeatureUid( Feature::Uid featureUid ) const;

	Plugin::Uid pluginUid( Feature::Uid featureUid ) const;


	void controlFeature( Feature::Uid featureUid,
						FeatureProviderInterface::Operation operation,
						const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces ) const;

	void startFeature( IkebackMasterInterface& master,
					   const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) const;
	void stopFeature( IkebackMasterInterface& master,
					  const Feature& feature,
					  const ComputerControlInterfaceList& computerControlInterfaces ) const;

	void handleFeatureMessage(ComputerControlInterface::Pointer computerControlInterface,
							  const FeatureMessage& message) const;
	void handleFeatureMessage(IkebackServerInterface& server,
							  const MessageContext& messageContext,
							  const FeatureMessage& message) const;
	void handleFeatureMessageFromWorker(IkebackServerInterface& server,
										const FeatureMessage& message) const;
	void handleFeatureMessage(IkebackWorkerInterface& worker, const FeatureMessage& message) const;

	void sendAsyncFeatureMessages(IkebackServerInterface& server, const MessageContext& messageContext) const;

	void startSessionFeatures(IkebackServerInterface& server) const;

	FeatureUidList activeFeatures( IkebackServerInterface& server ) const;

private:
	FeatureList m_features;
	FeatureUidList m_disabledFeaturesUids{};
	const FeatureList m_emptyFeatureList;
	QObjectList m_pluginObjects;
	FeatureProviderInterfaceList m_featurePluginInterfaces;
	const Feature m_dummyFeature;

};
