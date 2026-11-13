/*
 * IkebackWorker.h - basic implementation of Ikeback Worker
 *
 * Copyright (c) 2018-2026 Tobias Junghans <tobydox@veyon.io>
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

#include "IkebackCore.h"
#include "IkebackWorkerInterface.h"

class FeatureWorkerManagerConnection;

class IkebackWorker : public QObject, IkebackWorkerInterface
{
	Q_OBJECT
public:
	explicit IkebackWorker(QUuid featureUid, QObject* parent = nullptr);
	~IkebackWorker() override;

	bool sendFeatureMessageReply( const FeatureMessage& reply ) override;

	IkebackCore& core()
	{
		return m_core;
	}

private:
	IkebackCore m_core;
	FeatureWorkerManagerConnection* m_workerManagerConnection = nullptr;

} ;
