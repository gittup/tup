/* 
 * Shameless taken from udhcp as I think it's a good idea.
 * Signal pipe infrastructure. A reliable way of delivering signals.
 *
 * Russ Dill <Russ.Dill@asu.edu> December 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "logger.h"
#include "signals.h"

static int signal_pipe[2];

static void signal_handler (int sig)
{
	/* Silently ignore this signal and wait for it. This stops zombies.
	   We do this here instead of client.c so that we don't spam the log file
	   with "waiting on select messages" */
	if (sig == SIGCHLD) {
		wait (0);
		return;
	}

	if (send (signal_pipe[1], &sig, sizeof (sig), MSG_DONTWAIT) == -1)
		logger (LOG_ERR, "Could not send signal: %s", strerror (errno));
}

/* Call this before doing anything else. Sets up the socket pair
 * and installs the signal handler */
void signal_setup(void)
{
	int i;
	int flags;

	socketpair (AF_UNIX, SOCK_STREAM, 0, signal_pipe);

	/* Stop any scripts from inheriting us */
	for (i = 0; i < 2; i++)
		if ((flags = fcntl (signal_pipe[i], F_GETFD, 0)) == -1 ||
			fcntl (signal_pipe[i], F_SETFD, flags | FD_CLOEXEC) == -1)
			logger (LOG_ERR ,"fcntl: %s", strerror (errno));

	signal (SIGHUP, signal_handler);
	signal (SIGALRM, signal_handler);
	signal (SIGTERM, signal_handler);
	signal (SIGINT, signal_handler);
	signal (SIGCHLD, signal_handler);
}

/* Quick little function to setup the rfds. Will return the
 * max_fd for use with select. Limited in that you can only pass
 * one extra fd */
int signal_fd_set (fd_set *rfds, int extra_fd)
{
	FD_ZERO (rfds);
	FD_SET (signal_pipe[0], rfds);
	if (extra_fd >= 0)
		FD_SET (extra_fd, rfds);
	return signal_pipe[0] > extra_fd ? signal_pipe[0] : extra_fd;
}

/* Read a signal from the signal pipe. Returns 0 if there is
 * no signal, -1 on error (and sets errno appropriately), and
 * your signal on success */
int signal_read (const fd_set *rfds)
{
	int sig;

	if (! FD_ISSET (signal_pipe[0], rfds))
		return 0;

	if (read (signal_pipe[0], &sig, sizeof (sig)) == -1)
		return -1;

	return sig;
}

