/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2018  Mike Shal <marfey@gmail.com>
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

#ifndef tup_compat_h
#define tup_compat_h

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int compat_init(void);
void compat_lock_enable(void);
void compat_lock_disable(void);

#ifdef _WIN32
#define is_path_sep(str) ((str)[0] == '/' || (str)[0] == '\\')
#define is_full_path(str) (is_path_sep(str) || ((str)[0] != '\0' && (str)[1] == ':'))
#define is_root_path(str) ((str)[0] != '\0' && (str)[1] == ':' && (str)[2] == '\\' && (str)[3] == '\0')
#define SQL_NAME_COLLATION " collate nocase"
#define name_cmp stricmp
#define name_cmp_n strnicmp
/* Windows uses mtime, since ctime there is the creation time, not change time */
#define MTIME(b) b.st_mtime
#else
#define is_path_sep(ch) ((ch)[0] == '/')
#define is_full_path is_path_sep
#define is_root_path(str) (strcmp(str, "/") == 0)
#define SQL_NAME_COLLATION ""
#define name_cmp strcmp
#define name_cmp_n strncmp

#ifdef __APPLE__
/* OSX needs to check both ctime and mtime. "chmod" only affects ctime, while
 * saving a file using exchangedata() only affects mtime. Most other operations
 * affect both.
 */
#define MTIME_MAX(x,y) ((x)>(y) ? (x) : (y))
#define MTIME(b) MTIME_MAX(b.st_ctime, b.st_mtime)
#else
/* Use ctime on other platforms, since chmod will affect ctime, but not mtime.
 * Also on Linux, ctime will be updated when a file is renamed (t6058).
 */
#define MTIME(b) b.st_ctime
#endif
#endif

#endif
