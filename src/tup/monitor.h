/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2022  Mike Shal <marfey@gmail.com>
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

#ifndef tup_monitor_h
#define tup_monitor_h

#define AUTOUPDATE_PID "autoupdate pid"
#define MONITOR_PID_FILE ".tup/monitor.pid"

int monitor_supported(void);
int monitor(int argc, char **argv);
int stop_monitor(int restarting);
int monitor_get_pid(int restarting, int *pid);

enum {
	TUP_MONITOR_SHUTDOWN=0,
	TUP_MONITOR_RESTARTING=1,
	TUP_MONITOR_SAFE_SHUTDOWN=2,
};

#endif
