/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2018-2023  Mike Shal <marfey@gmail.com>
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

#include "ccache.h"
#include <string.h>

int is_ccache_path(const char *path)
{
	/* ccache paths */
	if(strstr(path, "/.ccache") != NULL)
		return 1;
	if(strstr(path, "/ccache-tmp/") != NULL)
		return 1;
#ifdef _WIN32
	if(strstr(path, "\\.ccache") != NULL)
		return 1;
#endif

	/* icecream file lcok. This file gets created if icecream falls back
	 * to local compilation mode.
	 */
	if(strncmp(path, "/tmp/.icecream-", 15) == 0)
		return 1;
	return 0;
}
