/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010  James McKaskill
 * Copyright (C) 2010-2014  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef tup_dllinject_h
#define tup_dllinject_h

#include <windows.h>

#ifdef BUILDING_DLLINJECT
# define DLLINJECT_API __declspec(dllexport)
#else
# define DLLINJECT_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

DLLINJECT_API BOOL WINAPI DllMain(HANDLE HDllHandle, DWORD Reason, LPVOID Reserved);

typedef struct remote_thread_t remote_thread_t;
typedef struct remote_thread32_t remote_thread32_t;

DLLINJECT_API DWORD tup_inject_init(remote_thread_t *r);

DLLINJECT_API void tup_inject_setexecdir(const char *dir);

DLLINJECT_API int tup_inject_dll(LPPROCESS_INFORMATION process, const char *depfilename,
				 const char *vardict_file);

#ifdef __cplusplus
}
#endif

#endif
