/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2010-2017  Mike Shal <marfey@gmail.com>
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

#include "tup/monitor.h"
#include <stdio.h>

int monitor_supported(void)
{
	return -1;
}

int monitor(int argc, char **argv)
{
	if(argc) {}
	if(argv) {}
	fprintf(stderr, "tup error: The file monitor is not supported on this platform.\n");
	return -1;
}

int stop_monitor(int restarting)
{
	if(restarting) {}
	fprintf(stderr, "tup error: The file monitor is not supported on this platform.\n");
	return -1;
}

int monitor_get_pid(int restarting, int *pid)
{
	if(restarting) {}
	*pid = -1;
	return 0;
}
