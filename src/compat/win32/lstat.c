/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2015-2017  Mike Shal <marfey@gmail.com>
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

#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
#include <stdio.h>
#include "tup/compat.h"

#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

static time_t filetime_to_epoch(FILETIME *ft)
{
	long long ticks = (((long long)ft->dwHighDateTime) << 32) + (long long)ft->dwLowDateTime;
	return (time_t)(ticks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}

static dev_t stat_dev_for(const wchar_t *wpathname)
{
	DWORD len;
	wchar_t *fullpath;

	len = GetFullPathNameW(wpathname, 0, NULL, NULL);
	fullpath = __builtin_alloca((len + 1) * sizeof(wchar_t));
	if(!GetFullPathNameW(wpathname, len, fullpath, NULL)) {
		fprintf(stderr, "tup error: GetFullPathNameW(\"%ls\") failed: 0x%08lx\n", wpathname, GetLastError());
		return 0;
	}
	if(fullpath[1] != L':') {
		return 0;
	}
	return fullpath[0] - L'A';
}

int win_lstat(const char *pathname, struct stat *buf)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	wchar_t wpathname[PATH_MAX];

	MultiByteToWideChar(CP_UTF8, 0, pathname, -1, wpathname, PATH_MAX);

	if(!GetFileAttributesExW(wpathname, GetFileExInfoStandard, &data)) {
		DWORD err = GetLastError();
		if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
			errno = ENOENT;
			return -1;
		}
		fprintf(stderr, "tup error: GetFileAttributesExW(\"%ls\") failed: 0x%08lx\n", wpathname, err);
		return -1;
	}

	buf->st_dev = stat_dev_for(wpathname);
	buf->st_rdev = buf->st_dev;
	buf->st_nlink = 1;
	buf->st_uid = 0;
	buf->st_gid = 0;
	buf->st_ino = 0;
	buf->st_ctime = filetime_to_epoch(&data.ftCreationTime);
	buf->st_mtime = filetime_to_epoch(&data.ftLastWriteTime);
	buf->st_atime = filetime_to_epoch(&data.ftLastAccessTime);
	buf->st_size = (((__int64)data.nFileSizeHigh) << 32) + (__int64)data.nFileSizeLow;

	buf->st_mode = 0;

	/* We could support symlinks on Windows with this, we'd just need to
	 * #define S_IFLNK and S_ISLNK.
	 */
	/* if(data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		buf->st_mode |= S_IFLNK;
	} */
	if(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		buf->st_mode |= S_IFDIR | S_IEXEC;
	} else {
		const char *ext = strrchr(pathname, '.');
		if(ext) {
			if(!stricmp(ext, ".exe") ||
			   !stricmp(ext, ".cmd") ||
			   !stricmp(ext, ".pif") ||
			   !stricmp(ext, ".bat") ||
			   !stricmp(ext, ".com")) {
				buf->st_mode |= S_IEXEC;
			}
		}
		buf->st_mode |= S_IFREG;
	}

	if(!(data.dwFileAttributes & FILE_ATTRIBUTE_READONLY)) {
		buf->st_mode |= S_IWRITE;
	}
	buf->st_mode |= S_IREAD;

	/* Propagate user permissions to the group/other bits. */
	buf->st_mode |= (buf->st_mode & S_IRWXU) >> 3;
	buf->st_mode |= (buf->st_mode & S_IRWXU) >> 6;

	return 0;
}
