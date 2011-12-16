/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011  Mike Shal <marfey@gmail.com>
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

#include "privs.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef _WIN32
int tup_drop_privs(void)
{
	return 0;
}
#else
static int privileges_dropped = 0;

int tup_privileged(void)
{
	if(privileges_dropped)
		return 1;
	return geteuid() == 0;
}

int tup_drop_privs(void)
{
	if(geteuid() == 0) {
		if(setgid(getgid()) != 0) {
			perror("setgid");
			return -1;
		}
		if(setuid(getuid()) != 0) {
			perror("setuid");
			return -1;
		}
		if(setuid(0) != -1) {
			fprintf(stderr, "tup error: Expected setuid(0) to fail.\n");
			return -1;
		}
		privileges_dropped = 1;
	}
	return 0;
}
#endif
