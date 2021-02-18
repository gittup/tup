/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2021  Mike Shal <marfey@gmail.com>
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

#include <stdio.h>
#include <windows.h>
#include <fcntl.h>
#include "dirpath.h"

int __wrap_open(const char *pathname, int flags, ...) ATTRIBUTE_USED;

int __wrap_open(const char *pathname, int flags, ...)
{
	mode_t mode = 0;
	HANDLE h;
	SECURITY_ATTRIBUTES sec;
	DWORD desiredAccess;
	DWORD creationDisposition;
	wchar_t wpathname[PATH_MAX];

	if(flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		DWORD attributes;

		attributes = GetFileAttributesA(pathname);

		/* If there was an error getting the file attributes, or if we
		 * are trying to open a normal file, we want to fall through to
		 * the CreateFile case. Only things that we know are
		 * directories go through the special dirpath logic.
		 */
		if(attributes != INVALID_FILE_ATTRIBUTES) {
			if(attributes & FILE_ATTRIBUTE_DIRECTORY) {
				return win32_add_dirpath(pathname);
			}
		}
	}
	if(flags & O_RDWR)
		desiredAccess = GENERIC_WRITE | GENERIC_READ;
	else if(flags & O_WRONLY)
		desiredAccess = GENERIC_WRITE;
	else
		desiredAccess = GENERIC_READ;

	if(flags & O_CREAT)
		creationDisposition = CREATE_ALWAYS;
	else
		creationDisposition = OPEN_EXISTING;

	memset(&sec, 0, sizeof(sec));
	sec.nLength = sizeof(sec);
	sec.lpSecurityDescriptor = NULL;
	sec.bInheritHandle = FALSE;

	/* Need to use CreateFile instead of __real_open so we can set the
	 * default handle inheritance to false. This way we can set the
	 * sub-process' stdout file to inheritable and set handle inheritance
	 * on the process itself.
	 */
	MultiByteToWideChar(CP_UTF8, 0, pathname, -1, wpathname, PATH_MAX);
	h = CreateFile(wpathname, desiredAccess, 0, &sec, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
	if(h == INVALID_HANDLE_VALUE) {
		errno = GetLastError();
		return -1;
	}
	if(mode)
		if(chmod(pathname, mode) < 0) {
			return -1;
		}
	return _open_osfhandle((intptr_t)h, 0);
}
