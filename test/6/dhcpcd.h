/*
 * dhcpcd - DHCP client daemon -
 * Copyright 2006-2007 Roy Marples <uberlord@gentoo.org>
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

#ifndef DHCPCD_H
#define DHCPCD_H

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <limits.h>
#include <stdbool.h>

#include "common.h"

#define DEFAULT_TIMEOUT     20
#define DEFAULT_LEASETIME   3600        /* 1 hour */

#define CLASS_ID_MAX_LEN    48
#define CLIENT_ID_MAX_LEN   48
#define USERCLASS_MAX_LEN   255

typedef struct options_t {
	char interface[IF_NAMESIZE];
	char hostname[MAXHOSTNAMELEN];
	int fqdn;
	char classid[CLASS_ID_MAX_LEN];
	int classid_len;
	char clientid[CLIENT_ID_MAX_LEN];
	int clientid_len;
	char userclass[USERCLASS_MAX_LEN];
	int userclass_len;
	unsigned leasetime;
	time_t timeout;
	int metric;

	bool doarp;
	bool dodns;
	bool dodomainname;
	bool dogateway;
	int  dohostname;
	bool domtu;
	bool donis;
	bool dontp;
	bool dolastlease;
	bool doinform;
	bool dorequest;
	bool doipv4ll;

	struct in_addr request_address;
	struct in_addr request_netmask;

	bool persistent;
	bool keep_address;
	bool daemonise;
	bool test;

	char *script;
	char pidfile[PATH_MAX];
} options_t;

#endif
