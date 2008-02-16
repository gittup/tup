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

#ifndef INTERFACE_H
#define INTERFACE_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <limits.h>
#include <stdbool.h>

#include "config.h"
#ifdef ENABLE_DUID
#ifndef DUID_LEN
#  define DUID_LEN				128 + 2
#endif
#endif

#define EUI64_ADDR_LEN			8
#define INFINIBAND_ADDR_LEN		20

/* Linux 2.4 doesn't define this */
#ifndef ARPHRD_IEEE1394
#  define ARPHRD_IEEE1394		24
#endif

/* The BSD's don't define this yet */
#ifndef ARPHRD_INFINIBAND
#  define ARPHRD_INFINIBAND		27
#endif

#define HWADDR_LEN				20	

typedef struct route_t
{
	struct in_addr destination; 
	struct in_addr netmask;
	struct in_addr gateway;
	struct route_t *next;
} route_t;

typedef struct address_t
{
	struct in_addr address;
	struct address_t *next;
} address_t;

typedef struct interface_t
{
	char name[IF_NAMESIZE];
	sa_family_t family;
	unsigned char hwaddr[HWADDR_LEN];
	int hwlen;
	bool arpable;
	unsigned short mtu;

	int fd;
	int buffer_length;

#ifdef __linux__
	int socket_protocol;
#endif

	char infofile[PATH_MAX];

	unsigned short previous_mtu;
	struct in_addr previous_address;
	struct in_addr previous_netmask;
	route_t *previous_routes;

	long start_uptime;

#ifdef ENABLE_DUID
	unsigned char duid[DUID_LEN];
	int duid_length;
#endif
} interface_t;

void free_address (address_t *addresses);
void free_route (route_t *routes);
unsigned long get_netmask (unsigned long addr);
char *hwaddr_ntoa (const unsigned char *hwaddr, int hwlen);

interface_t *read_interface (const char *ifname, int metric);
int get_mtu (const char *ifname);
int set_mtu (const char *ifname, short int mtu);

int add_address (const char *ifname, struct in_addr address,
				 struct in_addr netmask, struct in_addr broadcast);
int del_address (const char *ifname, struct in_addr address,
				 struct in_addr netmask);

int flush_addresses (const char *ifname);
unsigned long get_address (const char *ifname);
int has_address (const char *ifname, struct in_addr address);

int add_route (const char *ifname, struct in_addr destination,
			   struct in_addr netmask, struct in_addr gateway, int metric);
int change_route (const char *ifname, struct in_addr destination,
				  struct in_addr netmask, struct in_addr gateway, int metric);
int del_route (const char *ifname, struct in_addr destination,
			   struct in_addr netmask, struct in_addr gateway, int metric);

int inet_ntocidr (struct in_addr address);
struct in_addr inet_cidrtoaddr (int cidr);

#endif
