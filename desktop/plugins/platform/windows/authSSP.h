/*

 * authSSP.h - minimal replacement for the UltraVNC ms-logon SSP helper

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

#pragma once

#include <windows.h>
// The UltraVNC header we replaced pulled these in transitively; WindowsUserFunctions
// relies on them for NetGetDCName()/NetApiBufferFree()/NERR_Success and for
// GetUserNameExW()/NameDisplay. secext.h includes sspi.h, which refuses to
// compile unless one of the SECURITY_* flavours is selected first.
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <lm.h>
#include <secext.h>

// ikeback: ikeback signs students in by class/name/seat and never authenticates
// with Windows credentials, so the UltraVNC SSPI helper is no longer built.
// Its sources do not compile with recent MinGW toolchains (UNICODE signatures
// changed) and pull in a large legacy dependency for a code path we do not
// use. This stub keeps WindowsUserFunctions linkable and reports failure, so
// prepareLogon() takes its documented unsupported path.
extern "C" BOOL WINAPI SSPLogonUser( LPCWSTR szDomain, LPCWSTR szUser, LPCWSTR szPassword );
