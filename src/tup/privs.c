/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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

#ifdef _WIN32
int tup_privileged(void)
{
	/* Windows doesn't need to have tup be privileged in order to get full dependencies since
	 * it uses a different dependency mechanism. Therefore we can always report '1' here
	 * since we support that mode.
	 */
	return 1;
}

int tup_drop_privs(void)
{
	return 0;
}

int tup_temporarily_drop_privs(void)
{
	return 0;
}

int tup_restore_privs(void)
{
	return 0;
}
#else
#ifdef __linux__
#include <grp.h>
#endif
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
#ifdef __linux__
		/* On Linux this ensures that we don't have any lingering
		 * groups with root privileges after the setgid(). On OSX we
		 * still need some groups in order to actually do the FUSE
		 * mounts.
		 */
		setgroups(0, NULL);
#endif
		if(setgid(getgid()) != 0) {
			perror("setgid");
			return -1;
		}
		if(setuid(getuid()) != 0) {
			perror("setuid");
			return -1;
		}
		privileges_dropped = 1;
	}
	return 0;
}

static int temporarily_dropped_privileges = 0;
static gid_t original_egid;
static uid_t original_euid;

int tup_temporarily_drop_privs(void)
{
	if (geteuid() == 0) {
		original_egid = getegid();
		original_euid = geteuid();
		if (setegid(getgid()) != 0) {
			perror("setegid dropping");
			return -1;
		}
		if (seteuid(getuid()) != 0) {
			perror("seteuid dropping");
			return -1;
		}
		temporarily_dropped_privileges = 1;
	}
	return 0;
}

int tup_restore_privs(void)
{
	if (temporarily_dropped_privileges) {
		if (setegid(original_egid) != 0) {
			perror("setegid restoring");
			return -1;
		}
		if (seteuid(original_euid) != 0) {
			perror("seteuid restoring");
			return -1;
		}
	}
	return 0;
}
#endif
