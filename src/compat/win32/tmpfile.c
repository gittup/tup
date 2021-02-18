/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012-2021  Mike Shal <marfey@gmail.com>
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

/* tmpfile() on Windows will occasionally fail with "Permission denied" because
 * it seems not to be implemented properly. In particular, I've noticed this
 * happens in VS command-line shell (though not a bourne shell).
 *
 * This is a work-around that creates a temporary file in the getenv(TMP)
 * directory with a sequential ID. It maintains the delete-on-close semantics
 * of tmpfile() by setting the appropriate flag in CreateFile().
 */
#include <windows.h>
#include <stdio.h>
#include <unistd.h>

FILE *__wrap_tmpfile(void) ATTRIBUTE_USED;

FILE *__wrap_tmpfile(void)
{
	static _Atomic int num = 0;
	int fd;
	char filename[PATH_MAX];
	wchar_t wfilename[PATH_MAX];
	FILE *f = NULL;
	HANDLE h;

	snprintf(filename, sizeof(filename), "%s/tup-%i-%i.tmp", getenv("TMP"), getpid(), num);
	filename[sizeof(filename)-1] = 0;
	num++;

	/* Need to use CreateFile to be able to set it delete-on-close */
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, PATH_MAX);
	h = CreateFile(wfilename, GENERIC_WRITE | GENERIC_READ, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if(h == INVALID_HANDLE_VALUE)
		return NULL;

	/* Convert from HANDLE to FILE* */
	fd = _open_osfhandle((intptr_t)h, 0);
	if(fd < 0)
		return NULL;
	f = fdopen(fd, "w+");
	if(!f) {
		if(!close(fd)) {
			perror("close(fd) in tmpfile()");
			return NULL;
		}
	}

	return f;
}
