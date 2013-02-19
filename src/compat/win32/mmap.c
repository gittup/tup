/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2013  Mike Shal <marfey@gmail.com>
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

#include <windows.h>
#include <sys/mman.h>
#include <stdio.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	union {
		HANDLE h;
		intptr_t v;
	} handle;
	HANDLE mapping;

	if(addr) {/*unused */}
	if(prot) {/*unused */}
	if(flags) {/*unused */}
	if(offset) {/*unused */}

	handle.v = _get_osfhandle(fd);
	if(handle.h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "tup mmap windows error: Failed to call _get_osfhandle. Error code=0x%08lx\n", GetLastError());
		return MAP_FAILED;
	}

	mapping = CreateFileMapping(handle.h, NULL, PAGE_READONLY, 0, length, NULL);
	if(mapping == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "tup mmap windows error: Failed to call CreateFileMapping. Error code=0x%08lx\n", GetLastError());
		return MAP_FAILED;
	}
	return MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, length);
}
