/*
 * dhcpcd - DHCP client daemon -
 * Copyright 2006-2007 Roy Marples <uberlord@gentoo.org>
 * 
 * dhcpcd is an RFC2131 compliant DHCP client daemon.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* We need to define this to get kill on GNU systems */
#ifdef __linux__
#define _POSIX_SOURCE
#endif

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <paths.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "client.h"
#include "dhcpcd.h"
#include "dhcp.h"
#include "interface.h"
#include "logger.h"
#include "version.h"

#define STRINGINT(_string, _int) { \
	char *_tmp; \
	long _number = strtol (_string, &_tmp, 0); \
	errno = 0; \
	if ((errno != 0 && _number == 0) || _string == _tmp || \
		(errno == ERANGE && (_number == LONG_MAX || _number == LONG_MIN))) \
	{ \
		logger (LOG_ERR, "`%s' out of range", _string);; \
		exit (EXIT_FAILURE); \
	} \
	else \
	_int = (int) _number; \
}

static pid_t read_pid (const char *pidfile)
{
	FILE *fp;
	pid_t pid = 0;

	if ((fp = fopen (pidfile, "r")) == NULL) {
		errno = ENOENT;
		return 0;
	}

	fscanf (fp, "%d", &pid);
	fclose (fp);

	return pid;
}

static void usage ()
{
	printf ("usage: "PACKAGE" [-adknpEGHMNRTY] [-c script] [-h hostame] [-i classID]\n"
	        "              [-l leasetime] [-m metric] [-s ipaddress] [-t timeout]\n"
	        "              [-u userclass] [-F none | ptr | both] [-I clientID]\n");
}

int main(int argc, char **argv)
{
	options_t options;
	int doversion = 0;
	int dohelp = 0;
	int userclasses = 0;
	int opt;
	int option_index = 0;
	char prefix[IF_NAMESIZE + 3];
	pid_t pid;
	int debug = 0;
	int i;
	int pidfd = -1;
	int sig = 0;

	const struct option longopts[] = {
		{"arp",         no_argument,        NULL, 'a'},
		{"script",      required_argument,  NULL, 'c'},
		{"debug",       no_argument,        NULL, 'd'},
		{"hostname",    optional_argument,  NULL, 'h'},
		{"classid",     optional_argument,  NULL, 'i'},
		{"release",     no_argument,        NULL, 'k'},
		{"leasetime",   required_argument,  NULL, 'l'},
		{"metric",      required_argument,  NULL, 'm'},
		{"renew",       no_argument,        NULL, 'n'},
		{"persistent",  no_argument,        NULL, 'p'},
		{"inform",      optional_argument,  NULL, 's'},
		{"request",     optional_argument,  NULL, 'r'},
		{"timeout",     required_argument,  NULL, 't'},
		{"userclass",   required_argument,  NULL, 'u'},
		{"exit",        no_argument,        NULL, 'x'},
		{"lastlease",   no_argument,        NULL, 'E'},
		{"fqdn",        required_argument,  NULL, 'F'},
		{"nogateway",   no_argument,        NULL, 'G'},
		{"sethostname", no_argument,        NULL, 'H'},
		{"clientid",    optional_argument,  NULL, 'I'},
		{"noipv4ll",	no_argument,		NULL, 'L'},
		{"nomtu",       no_argument,        NULL, 'M'},
		{"nontp",       no_argument,        NULL, 'N'},
		{"nodns",       no_argument,        NULL, 'R'},
		{"test",        no_argument,        NULL, 'T'},
		{"nonis",       no_argument,        NULL, 'Y'},
		{"help",        no_argument,        &dohelp, 1},
		{"version",     no_argument,        &doversion, 1},
		{NULL,          0,                  NULL, 0}
	};

	/* Close any un-needed fd's */
	for (i = getdtablesize() - 1; i >= 3; --i)
		close (i);

	openlog (PACKAGE, LOG_PID, LOG_LOCAL0);

	memset (&options, 0, sizeof (options_t));
	options.script = (char *) DEFAULT_SCRIPT;
	snprintf (options.classid, CLASS_ID_MAX_LEN, "%s %s", PACKAGE, VERSION);
	options.classid_len = strlen (options.classid);

	options.doarp = true;
	options.dodns = true;
	options.domtu = true;
	options.donis = true;
	options.dontp = true;
	options.dogateway = true;
	options.daemonise = true;
	options.doinform = false;
	options.doipv4ll = true;
	options.timeout = DEFAULT_TIMEOUT;

	gethostname (options.hostname, sizeof (options.hostname));
	if (strcmp (options.hostname, "(none)") == 0 ||
		strcmp (options.hostname, "localhost") == 0)
		memset (options.hostname, 0, sizeof (options.hostname));

	/* Don't set any optional arguments here so we retain POSIX
	 * compatibility with getopt */
	while ((opt = getopt_long(argc, argv, "c:dh:i:kl:m:npr:s:t:u:xAEF:GHI:LMNRTY",
							  longopts, &option_index)) != -1)
	{
		switch (opt) {
			case 0:
				if (longopts[option_index].flag)
					break;
				logger (LOG_ERR, "option `%s' should set a flag",
						longopts[option_index].name);
				exit (EXIT_FAILURE);
				break;
			case 'c':
				options.script = optarg;
				break;
			case 'd':
				debug++;
				switch (debug) {
					case 1:
						setloglevel (LOG_DEBUG);
						break;
					case 2:
						options.daemonise = false;
						break;
				}
				break;
			case 'h':
				if (! optarg)
					memset (options.hostname, 0, sizeof (options.hostname));
				else if (strlen (optarg) > MAXHOSTNAMELEN) {
					logger (LOG_ERR, "`%s' too long for HostName string, max is %d",
							optarg, MAXHOSTNAMELEN);
					exit (EXIT_FAILURE);
				} else
					strlcpy (options.hostname, optarg, sizeof (options.hostname));
				break;
			case 'i':
				if (! optarg) {
					memset (options.classid, 0, sizeof (options.classid));
					options.classid_len = 0;
				} else if (strlen (optarg) > CLASS_ID_MAX_LEN) {
					logger (LOG_ERR, "`%s' too long for ClassID string, max is %d",
							optarg, CLASS_ID_MAX_LEN);
					exit (EXIT_FAILURE);
				} else
					options.classid_len = strlcpy (options.classid, optarg,
												   sizeof (options.classid));
				break;
			case 'k':
				sig = SIGHUP;
				break;
			case 'l':
				STRINGINT (optarg, options.leasetime);
				if (options.leasetime <= 0) {
					logger (LOG_ERR, "leasetime must be a positive value");
					exit (EXIT_FAILURE);
				}
				break;
			case 'm':
				STRINGINT (optarg, options.metric);
				break;
			case 'n':
				sig = SIGALRM;
				break;
			case 'p':
				options.persistent = true;
				break;
			case 's':
				options.doinform = true;
				if (! optarg || strlen (optarg) == 0) {
					options.request_address.s_addr = 0;
					break;
				} else {
					char *slash = strchr (optarg, '/');
					if (slash) {
						int cidr;
						/* nullify the slash, so the -r option can read the
						 * address */
						*slash++ = '\0';
						if (sscanf (slash, "%d", &cidr) != 1) {
							logger (LOG_ERR, "`%s' is not a valid CIDR", slash);
							exit (EXIT_FAILURE);
						}
						options.request_netmask = inet_cidrtoaddr (cidr);
					}
					/* fall through */
				}
			case 'r':
				if (! options.doinform)
					options.dorequest = true;
				if (strlen (optarg) > 0 &&
					! inet_aton (optarg, &options.request_address))
				{ 
					logger (LOG_ERR, "`%s' is not a valid IP address", optarg);
					exit (EXIT_FAILURE);
				}
				break;
			case 't':
				STRINGINT (optarg, options.timeout);
				if (options.timeout < 0) {
					logger (LOG_ERR, "timeout must be a positive value");
					exit (EXIT_FAILURE);
				}
				break;
			case 'u':
				{
					int offset = 0;
					for (i = 0; i < userclasses; i++)
						offset += (int) options.userclass[offset] + 1;
					if (offset + 1 + strlen (optarg) > USERCLASS_MAX_LEN) {
						logger (LOG_ERR, "userclass overrun, max is %d",
								USERCLASS_MAX_LEN);
						exit (EXIT_FAILURE);
					}
					userclasses++;
					memcpy (options.userclass + offset + 1 , optarg, strlen (optarg));
					options.userclass[offset] = strlen (optarg);
					options.userclass_len += (strlen (optarg)) + 1;
				}
				break;
			case 'x':
				sig = SIGTERM;
				break;
			case 'A':
#ifndef ENABLE_ARP
				logger (LOG_ERR, "arp support not compiled into dhcpcd");
				exit (EXIT_FAILURE);
#endif
				options.doarp = false;
				break;
			case 'E':
#ifndef ENABLE_INFO
				logger (LOG_ERR, "info support not compiled into dhcpcd");
				exit (EXIT_FAILURE);
#endif
				options.dolastlease = true;
				break;
			case 'F':
				if (strncmp (optarg, "none", strlen (optarg)) == 0)
					options.fqdn = FQDN_NONE;
				else if (strncmp (optarg, "ptr", strlen (optarg)) == 0)
					options.fqdn = FQDN_PTR;
				else if (strncmp (optarg, "both", strlen (optarg)) == 0)
					options.fqdn = FQDN_BOTH;
				else {
					logger (LOG_ERR, "invalid value `%s' for FQDN", optarg);
					exit (EXIT_FAILURE);
				}
				break;
			case 'G':
				options.dogateway = false;
				break;
			case 'H':
				options.dohostname++;
				break;
			case 'I':
				if (optarg) {
					if (strlen (optarg) > CLIENT_ID_MAX_LEN) {
						logger (LOG_ERR, "`%s' is too long for ClientID, max is %d",
								optarg, CLIENT_ID_MAX_LEN);
						exit (EXIT_FAILURE);
					}
					options.clientid_len = strlcpy (options.clientid, optarg,
													sizeof (options.clientid));
					/* empty string disabled duid */
					if (options.clientid_len == 0)
						options.clientid_len = -1;
				} else {
					memset (options.clientid, 0, sizeof (options.clientid));
					options.clientid_len = -1;
				}
				break;
			case 'L':
				options.doipv4ll = false;
				break;
			case 'M':
				options.domtu = false;
				break;
			case 'N':
				options.dontp = false;
				break;
			case 'R':
				options.dodns = false;
				break;
			case 'T':
#ifndef ENABLE_INFO
				logger (LOG_ERR, "info support not compiled into dhcpcd");
				exit (EXIT_FAILURE);
#endif
				options.test = true;
				options.persistent = true;
				break;
			case 'Y':
				options.donis = false;
				break;
			case '?':
				usage ();
				exit (EXIT_FAILURE);
			default:
				usage ();
				exit (EXIT_FAILURE);
		}
	}
	if (doversion)
		printf (""PACKAGE" "VERSION"\n");

	if (dohelp)
		usage ();

	if (optind < argc) {
		if (strlen (argv[optind]) > IF_NAMESIZE) {
			logger (LOG_ERR, "`%s' is too long for an interface name (max=%d)",
					argv[optind], IF_NAMESIZE);
			exit (EXIT_FAILURE);
		}
		strlcpy (options.interface, argv[optind],
				 sizeof (options.interface));
	} else {
		/* If only version was requested then exit now */
		if (doversion || dohelp)
			exit (EXIT_SUCCESS);

		logger (LOG_ERR, "no interface specified");
		exit (EXIT_FAILURE);
	}

	if (strchr (options.hostname, '.')) {
		if (options.fqdn == FQDN_DISABLE)
			options.fqdn = FQDN_BOTH;
	} else
		options.fqdn = FQDN_DISABLE;

	if (options.request_address.s_addr == 0 && options.doinform) {
		if ((options.request_address.s_addr = get_address (options.interface)) != 0)
			options.keep_address = true;
	}

	if (geteuid ()) {
		logger (LOG_ERR, "you need to be root to run "PACKAGE);
		exit (EXIT_FAILURE);
	}

	snprintf (prefix, IF_NAMESIZE, "%s: ", options.interface);
	setlogprefix (prefix);
	snprintf (options.pidfile, sizeof (options.pidfile), PIDFILE,
			  options.interface);

	chdir ("/");
	umask (022);

	if (mkdir (CONFIGDIR, S_IRUSR |S_IWUSR |S_IXUSR | S_IRGRP | S_IXGRP
			   | S_IROTH | S_IXOTH) && errno != EEXIST )
	{
		logger (LOG_ERR, "mkdir(\"%s\",0): %s\n", CONFIGDIR, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (mkdir (ETCDIR, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP
			   | S_IROTH | S_IXOTH) && errno != EEXIST )
	{
		logger (LOG_ERR, "mkdir(\"%s\",0): %s\n", ETCDIR, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (options.test) {
		if (options.dorequest || options.doinform) {
			logger (LOG_ERR, "cannot test with --inform or --request");
			exit (EXIT_FAILURE);
		}

		if (options.dolastlease) {
			logger (LOG_ERR, "cannot test with --lastlease");
			exit (EXIT_FAILURE);
		}

		if (sig != 0) {
			logger (LOG_ERR, "cannot test with --release or --renew");
			exit (EXIT_FAILURE);
		}
	}

	if (sig != 0 ) {
		int killed = -1;
		pid = read_pid (options.pidfile);
		if (pid != 0)
			logger (LOG_INFO, "sending signal %d to pid %d", sig, pid);

		if (! pid || (killed = kill (pid, sig)))
			logger (sig == SIGALRM ? LOG_INFO : LOG_ERR, ""PACKAGE" not running");

		if (pid != 0 && (sig != SIGALRM || killed != 0))
			unlink (options.pidfile);

		if (killed == 0)
			exit (EXIT_SUCCESS);

		if (sig != SIGALRM)
			exit (EXIT_FAILURE);
	}

	if (! options.test) {
		if ((pid = read_pid (options.pidfile)) > 0 && kill (pid, 0) == 0) {
			logger (LOG_ERR, ""PACKAGE" already running on pid %d (%s)",
					pid, options.pidfile);
			exit (EXIT_FAILURE);
		}

		pidfd = open (options.pidfile, O_WRONLY | O_CREAT | O_NONBLOCK, 0660);
		if (pidfd == -1) {
			logger (LOG_ERR, "open `%s': %s", options.pidfile, strerror (errno));
			exit (EXIT_FAILURE);
		}

		/* Lock the file so that only one instance of dhcpcd runs on an interface */
		if (flock (pidfd, LOCK_EX | LOCK_NB) == -1) {
			logger (LOG_ERR, "flock `%s': %s", options.pidfile, strerror (errno));
			exit (EXIT_FAILURE);
		}

		/* dhcpcd.sh should not interhit this fd */
		if ((i = fcntl (pidfd, F_GETFD, 0)) == -1 ||
			fcntl (pidfd, F_SETFD, i | FD_CLOEXEC) == -1)
			logger (LOG_ERR, "fcntl: %s", strerror (errno));

		logger (LOG_INFO, PACKAGE " " VERSION " starting");
	}

	/* Seed random */
	srandomdev ();

	if (dhcp_run (&options, &pidfd)) {
		if (pidfd > -1)
			close (pidfd);
		unlink (options.pidfile);
		exit (EXIT_FAILURE);
	}

	exit (EXIT_SUCCESS);
}
