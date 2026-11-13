/*

 * authSSP.cpp - minimal replacement for the UltraVNC ms-logon SSP helper

 * Copyright (c) 2026 zk26

 * This file is part of ikeback - https://github.com/zk26/ikeback

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program (see COPYING); if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

#include "authSSP.h"

extern "C" BOOL WINAPI SSPLogonUser( LPCWSTR, LPCWSTR, LPCWSTR )
{
	SetLastError( ERROR_NOT_SUPPORTED );
	return FALSE;
}
