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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#ifdef __linux__
#include <netinet/ether.h>
#include <netpacket/packet.h>
#endif
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "arp.h"
#include "interface.h"
#include "logger.h"
#include "socket.h"

/* These are really for IPV4LL */
#define NPROBES                 3
#define PROBE_INTERVAL          200000
#define NCLAIMS                 2
#define CLAIM_INTERVAL          200000

/* Linux does not seem to define these handy macros */
#ifndef ar_sha
#define ar_sha(ap) (((unsigned char *) ((ap) + 1)) + 0)
#define ar_spa(ap) (((unsigned char *) ((ap) + 1)) + (ap)->ar_hln)
#define ar_tha(ap) (((unsigned char *) ((ap) + 1)) + \
					(ap)->ar_hln + (ap)->ar_pln)
#define ar_tpa(ap) (((unsigned char *) ((ap) + 1)) + \
					2 * (ap)->ar_hln + (ap)->ar_pln)
#endif

#ifndef arphdr_len
#define arphdr_len2(ar_hln, ar_pln) (sizeof (struct arphdr) + \
									 2 * (ar_hln) + 2 * (ar_pln))
#define arphdr_len(ap) (arphdr_len2 ((ap)->ar_hln, (ap)->ar_pln))
#endif

#ifdef ENABLE_ARP

static int send_arp (const interface_t *iface, int op, struct in_addr sip,
					 const unsigned char *taddr, struct in_addr tip)
{
	struct arphdr *arp;
	int arpsize = arphdr_len2 (iface->hwlen, sizeof (struct in_addr));
	int retval;

	arp = xmalloc (arpsize);
	memset (arp, 0, arpsize);

	arp->ar_hrd = htons (iface->family);
	arp->ar_pro = htons (ETHERTYPE_IP);
	arp->ar_hln = iface->hwlen;
	arp->ar_pln = sizeof (struct in_addr);
	arp->ar_op = htons (op);
	memcpy (ar_sha (arp), iface->hwaddr, arp->ar_hln);
	memcpy (ar_spa (arp), &sip, arp->ar_pln);
	if (taddr)
		memcpy (ar_tha (arp), taddr, arp->ar_hln); 
	memcpy (ar_tpa (arp), &tip, arp->ar_pln);

	retval = send_packet (iface, ETHERTYPE_ARP,
						  (unsigned char *) arp, arphdr_len (arp));
	free (arp);
	return (retval);
}

int arp_claim (interface_t *iface, struct in_addr address)
{
	struct arphdr *reply = NULL;
	long timeout = 0;
	unsigned char *buffer;
	int retval = -1;
	int nprobes = 0;
	int nclaims = 0;
	struct in_addr null_address;

	if (! iface)
		return (-1);

	if (! iface->arpable) {
		logger (LOG_DEBUG, "interface `%s' is not ARPable", iface->name);
		return (0);
	}

	logger (LOG_INFO, "checking %s is available on attached networks",
			inet_ntoa (address));

	if (! open_socket (iface, true))
		return (-1);

	memset (&null_address, 0, sizeof (struct in_addr));

	buffer = xmalloc (iface->buffer_length);
	reply = xmalloc (iface->buffer_length);

	while (1) {
		struct timeval tv;
		int bufpos = -1;
		int buflen = iface->buffer_length;
		fd_set rset;
		int bytes;
		int s = 0;
		struct timeval stopat;
		struct timeval now;

		/* Only select if we have a timeout */
		if (timeout > 0) {
			tv.tv_sec = 0; 
			tv.tv_usec = timeout;

			FD_ZERO (&rset);
			FD_SET (iface->fd, &rset);

			errno = 0;
			if ((s = select (FD_SETSIZE, &rset, NULL, NULL, &tv)) == -1) {
				if (errno != EINTR)
					logger (LOG_ERR, "select: `%s'", strerror (errno));
				break;
			}
		}

		/* Timed out */
		if (s == 0) {
			if (nprobes < NPROBES) {
				nprobes ++;
				timeout = PROBE_INTERVAL;
				logger (LOG_DEBUG, "sending ARP probe #%d", nprobes);
				send_arp (iface, ARPOP_REQUEST, null_address, NULL, address);
			} else if (nclaims < NCLAIMS) {
				nclaims ++;
				timeout = CLAIM_INTERVAL;
				logger (LOG_DEBUG, "sending ARP claim #%d", nclaims);
				send_arp (iface, ARPOP_REQUEST, address, iface->hwaddr, address);
			} else {
				/* No replies, so done */
				retval = 0;
				break;
			}

			/* Setup our stop time */
			if (get_time (&stopat) != 0)
				break;
			stopat.tv_usec += timeout;

			continue;
		}

		/* We maybe ARP flooded, so check our time */
		if (get_time (&now) != 0)
			break;
		if (timercmp (&now, &stopat, >)) {
			timeout = 0;
			continue;
		}

		if (! FD_ISSET (iface->fd, &rset))
			continue;

		memset (buffer, 0, buflen);
		while (bufpos != 0)	{
			union {
				unsigned char *c;
				struct in_addr *a;
			} rp;
			union {
				unsigned char *c;
				struct ether_addr *a;
			} rh;

			memset (reply, 0, iface->buffer_length);
			if ((bytes = get_packet (iface, (unsigned char *) reply,
									 buffer,
									 &buflen, &bufpos)) == -1)
				break;

			/* Only these types are recognised */
			if (reply->ar_op != htons (ARPOP_REPLY))
				continue;

			/* Protocol must be IP. */
			if (reply->ar_pro != htons (ETHERTYPE_IP))
				continue;
			if (reply->ar_pln != sizeof (struct in_addr))
				continue;
			if ((unsigned) bytes < sizeof (reply) + 
				2 * (4 + reply->ar_hln))
				continue;

			rp.c = (unsigned char *) ar_spa (reply);
			rh.c = (unsigned char *) ar_sha (reply);

			/* Ensure the ARP reply is for the address we asked for */
			if (rp.a->s_addr != address.s_addr)
				continue;

			/* Some systems send a reply back from our hwaddress - weird */
			if (reply->ar_hln == iface->hwlen &&
				memcmp (rh.c, iface->hwaddr, iface->hwlen) == 0)
				continue;

			logger (LOG_ERR, "ARPOP_REPLY received from %s (%s)",
					inet_ntoa (*rp.a),
					hwaddr_ntoa (rh.c, reply->ar_hln));
			retval = -1;
			goto eexit;
		}
	}

eexit:
	close (iface->fd);
	iface->fd = -1;
	free (buffer);
	free (reply);
	return (retval);
}
#endif
