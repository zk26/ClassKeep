/*
 * interception.h - minimal compatibility header for the optional Interception
 *                 keyboard/mouse driver API.
 *
 * ikeback build shim. The desktop build expects `-linterception`, a library
 * that is not part of this repository (3rdparty/interception/ only contains
 * install-interception.exe and two .bat files) and is NOT available in MSYS2
 * UCRT64. Without this header the mandatory `windows-platform` plugin cannot
 * be built, and IkebackCore then aborts at startup with
 * "PlatformPluginManager: no platform plugin available!".
 *
 * Interception is an OPTIONAL driver: WindowsPlatformConfiguration::
 * useInterceptionDriver() is off by default, and the plugin falls back to
 * low-level Windows hooks. These no-op stubs therefore preserve the default
 * behaviour while removing the unavailable link dependency. No code from the
 * upstream Interception project is included here.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void* InterceptionContext;
typedef int InterceptionDevice;
typedef unsigned short InterceptionFilter;
typedef int (*InterceptionPredicate)( InterceptionDevice );

#define INTERCEPTION_FILTER_KEY_ALL   ((InterceptionFilter) 0xFFFF)
#define INTERCEPTION_FILTER_MOUSE_ALL ((InterceptionFilter) 0xFFFF)

static inline InterceptionContext interception_create_context( void )
{
	return (InterceptionContext) 0;
}

static inline void interception_destroy_context( InterceptionContext context )
{
	(void) context;
}

static inline void interception_set_filter( InterceptionContext context,
										   InterceptionPredicate predicate,
										   InterceptionFilter filter )
{
	(void) context;
	(void) predicate;
	(void) filter;
}

#ifdef __cplusplus
}
#endif
