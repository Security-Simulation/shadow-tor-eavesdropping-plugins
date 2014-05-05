/* Shadow Overloading Library
 * Copyright (C) 2014 Davide Berardi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define SHADOW_ENUMERATOR(basename) enum {\
	basename##_INFO,\
	basename##_MESSAGE,\
	basename##_DEBUG,\
	basename##_WARNING,\
	basename##_CRITICAL\
}

typedef SHADOW_ENUMERATOR(SHADOW_LOG_LEVEL) ShadowLogLevel;

typedef void (*ShadowLogFunc)(ShadowLogLevel level,
			      const char *function, const char *format, ...);

typedef int (*ShadowRegPlug)(void (*plugin_new)(int argc, char **argv),
			     void (*plugin_free)(void),
			     void (*plugin_ready)(void));

typedef struct {
	ShadowLogFunc log;
	ShadowRegPlug registerPlugin;
} ShadowFunctionTable;
