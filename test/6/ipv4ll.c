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

#include <errno.h>
#include <stdlib.h>

#include "config.h"
#include "arp.h"
#include "ipv4ll.h"

#ifdef ENABLE_IPV4LL

#ifndef ENABLE_ARP
#error IPV4LL requires ARP
#endif

#define IPV4LL_LEASETIME 10 

int ipv4ll_get_address (interface_t *iface, dhcp_t *dhcp) {
	struct in_addr addr;

	while (1) {
		addr.s_addr = htonl (LINKLOCAL_ADDR |
							 ((abs (random ()) % 0xFD00) + 0x0100));
		errno = 0;
		if (! arp_claim (iface, addr))
			break;
		/* Our ARP may have been interrupted */
		if (errno == EINTR)
			return (-1);
	}

	dhcp->address.s_addr = addr.s_addr;
	dhcp->netmask.s_addr = htonl (LINKLOCAL_MASK);
	dhcp->broadcast.s_addr = htonl (LINKLOCAL_BRDC);

	/* Finally configure some DHCP like lease times */
	dhcp->leasetime = IPV4LL_LEASETIME;
	dhcp->renewaltime = (dhcp->leasetime * 0.5);
	dhcp->rebindtime = (dhcp->leasetime * 0.875);

	return (0);
}

#endif
