/*
 * IkebackMaster.cpp - management of application-global instances
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

#include <QHostAddress>
#include <QMessageBox>

#include "IkebackMaster.h"
#include "BuiltinFeatures.h"
#include "ComputerControlListModel.h"
#include "ComputerMonitoringModel.h"
#include "FeatureManager.h"
#include "IkebackConfiguration.h"
#include "MainWindow.h"
#include "ComputerManager.h"
#include "MonitoringMode.h"
#include "UserConfig.h"
#include "PluginManager.h"


IkebackMaster::IkebackMaster( QObject* parent ) :
	IkebackMasterInterface( parent ),
	m_userConfig(new UserConfig(QStringLiteral("ikebackMaster"))),
	m_features( featureList() ),
	m_computerManager( new ComputerManager( *m_userConfig, this ) ),
	m_computerControlListModel( new ComputerControlListModel( this, this ) ),
	m_computerMonitoringModel( new ComputerMonitoringModel( this ) ),
	m_localSessionControlInterface( Computer( NetworkObject::Uid::createUuid(),
											  QStringLiteral("localhost"),
											  QStringLiteral("%1").
											  arg( QHostAddress( QHostAddress::LocalHost ).toString() ) ),
									IkebackCore::config().ikebackServerPort() + IkebackCore::sessionId(),
									this ),
	m_mainWindow( nullptr ),
	m_currentMode( IkebackCore::builtinFeatures().monitoringMode().feature().uid() )
{
	if (m_userConfig->isStoreWritable() == false)
	{
		QMessageBox::information(nullptr,
								 tr("No write access"),
								 tr("Could not save your personal settings! Please check the user configuration "
									"file path using Ikeback Configurator."));
	}

	connect(m_computerControlListModel, &ComputerControlListModel::modelAboutToBeReset,
			this, &IkebackMaster::computerControlListModelAboutToReset);

	if( IkebackCore::config().enforceSelectedModeForClients() )
	{
		connect( m_computerControlListModel, &ComputerControlListModel::activeFeaturesChanged,
				 this, &IkebackMaster::enforceDesignatedMode );
		connect(m_computerControlListModel, &ComputerControlListModel::stateChanged,
				this, &IkebackMaster::enforceDesignatedMode);
	}

	m_localSessionControlInterface.start({}, ComputerControlInterface::UpdateMode::Disabled);

	// attach computer list model to proxy model
	m_computerMonitoringModel->setSourceModel( m_computerControlListModel );
	m_computerMonitoringModel->setSortRole( Qt::InitialSortOrderRole );
	m_computerMonitoringModel->setStateRole( ComputerControlListModel::StateRole );
	m_computerMonitoringModel->sort( 0 );

	m_mainWindow = new MainWindow( *this );
}



IkebackMaster::~IkebackMaster()
{
	shutdown();

	delete m_mainWindow;

	delete m_computerManager;

	m_userConfig->flushStore();
	delete m_userConfig;
}



FeatureList IkebackMaster::subFeatures( Feature::Uid parentFeatureUid ) const
{
	const auto disabledFeatures = IkebackCore::config().disabledFeatures();
	if( disabledFeatures.contains( parentFeatureUid.toString() ) )
	{
		return {};
	}

	const auto& relatedFeatures = IkebackCore::featureManager().relatedFeatures( parentFeatureUid );

	FeatureList subFeatures;
	subFeatures.reserve( relatedFeatures.size() );

	for( const auto& relatedFeature : relatedFeatures )
	{
		if( relatedFeature.testFlag( Feature::Flag::Master ) &&
			relatedFeature.parentUid() == parentFeatureUid &&
			disabledFeatures.contains( relatedFeature.uid().toString() ) == false )
		{
			subFeatures += relatedFeature;
		}
	}

	return subFeatures;
}



FeatureList IkebackMaster::allFeatures() const
{
	FeatureList featureList;

	for( const auto& feature : std::as_const( features() ) )
	{
		featureList.append(feature); // clazy:exclude=reserve-candidates
		const auto modeSubFeatures = subFeatures( feature.uid() );
		for( const auto& subFeature : modeSubFeatures )
		{
			featureList.append( subFeature );
		}
	}

	return featureList;
}



QWidget* IkebackMaster::mainWindow()
{
	return m_mainWindow;
}



Configuration::Object* IkebackMaster::userConfigurationObject()
{
	return m_userConfig;
}



void IkebackMaster::reloadSubFeatures()
{
	m_mainWindow->reloadSubFeatures();
}



const ComputerControlInterfaceList& IkebackMaster::allComputerControlInterfaces() const
{
	return m_computerControlListModel->computerControlInterfaces();
}



ComputerControlInterfaceList IkebackMaster::selectedComputerControlInterfaces() const
{
	return m_mainWindow->selectedComputerControlInterfaces();
}



ComputerControlInterfaceList IkebackMaster::filteredComputerControlInterfaces() const
{
	const auto rowCount = m_computerMonitoringModel->rowCount();

	ComputerControlInterfaceList computerControlInterfaces;
	computerControlInterfaces.reserve( rowCount );

	for( int i = 0; i < rowCount; ++i )
	{
		const auto index = m_computerMonitoringModel->index( i, 0 );
		const auto sourceIndex = m_computerMonitoringModel->mapToSource( index );
		computerControlInterfaces.append( m_computerControlListModel->computerControlInterface( sourceIndex ) );
	}

	return computerControlInterfaces;
}



void IkebackMaster::runFeature( const Feature& feature )
{
	const auto computerControlInterfaces = filteredComputerControlInterfaces();

	if( feature.testFlag( Feature::Flag::Mode ) )
	{
		stopAllFeatures( computerControlInterfaces );

		if( m_currentMode == feature.uid() ||
			subFeatures( feature.uid() ).contains( IkebackCore::featureManager().feature( m_currentMode ) ) )
		{
			const Feature& monitoringModeFeature = IkebackCore::builtinFeatures().monitoringMode().feature();

			IkebackCore::featureManager().startFeature( *this, monitoringModeFeature, computerControlInterfaces );
			m_currentMode = monitoringModeFeature.uid();
		}
		else
		{
			IkebackCore::featureManager().startFeature( *this, feature, computerControlInterfaces );
			m_currentMode = feature.uid();
		}
	}
	else
	{
		IkebackCore::featureManager().startFeature( *this, feature, computerControlInterfaces );
	}
}



void IkebackMaster::enforceDesignatedMode( const QModelIndex& index )
{
	auto controlInterface = m_computerControlListModel->computerControlInterface( index );
	if (controlInterface && controlInterface->state() == ComputerControlInterface::State::Connected)
	{
		const auto designatedModeFeature = controlInterface->designatedModeFeature();

		if( designatedModeFeature.isNull() == false &&
			designatedModeFeature != IkebackCore::builtinFeatures().monitoringMode().feature().uid() &&
			controlInterface->activeFeatures().contains( designatedModeFeature ) == false &&
			controlInterface->activeFeatures().contains( IkebackCore::featureManager().metaFeatureUid(designatedModeFeature) ) == false )
		{
			IkebackCore::featureManager().startFeature( *this, IkebackCore::featureManager().feature(designatedModeFeature),
													  { controlInterface } );
		}
	}
}



void IkebackMaster::stopAllFeatures( const ComputerControlInterfaceList& computerControlInterfaces )
{
	const auto features = allFeatures();

	for( const auto& feature : features )
	{
		IkebackCore::featureManager().stopFeature( *this, feature, computerControlInterfaces );
	}
}



void IkebackMaster::shutdown()
{
	stopAllFeatures( m_computerControlListModel->computerControlInterfaces() );

	const auto allMessageQueuesEmpty = [&]() {
		for( const auto& computerControlInterface : m_computerControlListModel->computerControlInterfaces() )
		{
			if( computerControlInterface->isMessageQueueEmpty() == false )
			{
				return false;
			}
		}

		return true;
	};

	static constexpr auto MessageQueueWaitTimeout = 60 * 1000;
	static constexpr auto MessageQueuePollInterval = 10;

	QElapsedTimer messageQueueWaitTimer;
	messageQueueWaitTimer.start();

	while( allMessageQueuesEmpty() == false &&
		   messageQueueWaitTimer.elapsed() < MessageQueueWaitTimeout )
	{
		QThread::msleep(MessageQueuePollInterval);
	}

	vDebug() << "finished in" << messageQueueWaitTimer.elapsed() << "ms";
}



FeatureList IkebackMaster::featureList() const
{
	FeatureList features;

	const auto disabledFeatures = IkebackCore::config().disabledFeatures();
	auto pluginUids = IkebackCore::pluginManager().pluginUids();

	std::sort(pluginUids.begin(), pluginUids.end(), [](const Plugin::Uid a, const Plugin::Uid b) {
		return a.toString() < b.toString();
	});

	const auto addFeatures = [&]( const std::function<bool(const Feature&)>& extraFilter )
	{
		for(const auto& pluginUid : std::as_const(pluginUids))
		{
			for( const auto& feature : IkebackCore::featureManager().features( pluginUid ) )
			{
				if( feature.testFlag( Feature::Flag::Master ) &&
					feature.testFlag( Feature::Flag::Meta ) == false &&
					feature.parentUid().isNull() &&
					disabledFeatures.contains( feature.uid().toString() ) == false &&
					extraFilter( feature ) )
				{
					features += feature;
				}
			}
		}
	};

	addFeatures( []( const Feature& feature ) { return feature.testFlag( Feature::Flag::Mode ); } );
	addFeatures( []( const Feature& feature ) { return feature.testFlag( Feature::Flag::Mode ) == false; } );

	return features;
}
