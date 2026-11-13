/*

 * cadthread-stub.cpp - CAD helper placeholder for the pinned UltraVNC sources

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

#include "cadthread.h"

// ikeback only needs the key mapping helpers from the pinned UltraVNC tree.
// vnckeymap.cpp still spawns vncCad::Cadthread for the Ctrl-Alt-Del path, whose
// implementation lives in a translator that is not part of this target.
// ikeback sends Ctrl-Alt-Del through its own input controller, so the thread
// body is a no-op here instead of pulling in the whole upstream module.
DWORD WINAPI vncCad::Cadthread( LPVOID )
{
	return 0;
}
