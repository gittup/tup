/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2026  Mike Shal <marfey@gmail.com>
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

#ifndef monitor_common_h
#define monitor_common_h

#include <pthread.h>
#include <sys/types.h>

#define AUTOUPDATE_EXIT -2
#define AUTOUPDATE_NONE -1

extern char **update_argv;
extern int update_argc;
extern int autoupdate_flag;
extern int autoparse_flag;
extern pthread_mutex_t autoupdate_lock;
extern pthread_cond_t autoupdate_cond;
extern pid_t autoupdate_pid;

int monitor_set_pid(int pid);
void monitor_parse_args(int argc, char **argv, int *foreground);
int autoupdate_enabled(void);
int autoparse_enabled(void);
int autoupdate(const char *cmd);
void *wait_thread(void *arg);

#endif
