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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>

/* Netlink suff */
#ifdef __linux__
#include <asm/types.h> /* Needed for 2.4 kernels */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>
#else
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#endif

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "interface.h"
#include "logger.h"

void free_address (address_t *addresses)
{
	address_t *p = addresses;
	address_t *n = NULL;

	if (! addresses)
		return;

	while (p) {
		n = p->next;
		free (p);
		p = n;
	}
}

void free_route (route_t *routes)
{
	route_t *p = routes;
	route_t *n = NULL;

	if (! routes)
		return;

	while (p) {
		n = p->next;
		free (p);
		p = n;
	}
}

int inet_ntocidr (struct in_addr address)
{
	int cidr = 0;
	uint32_t mask = htonl (address.s_addr);

	while (mask) {
		cidr++;
		mask <<= 1;
	}

	return (cidr);
}

struct in_addr inet_cidrtoaddr (int cidr) {
	struct in_addr addr;
	int ocets;

	if (cidr == 0)
		ocets = 0;
	else if (cidr < 9)
		ocets = 1;
	else if (cidr < 17)
		ocets = 2;
	else if (cidr < 25)
		ocets = 3;
	else
		ocets = 4;

	memset (&addr, 0, sizeof (struct in_addr));
	if (ocets > 0) {
		memset (&addr.s_addr, 255, ocets - 1);
		memset ((unsigned char *) &addr.s_addr + (ocets - 1),
				(256 - (1 << (32 - cidr) % 8)), 1);
	}

	return (addr);
}

unsigned long get_netmask (unsigned long addr)
{
	unsigned long dst;

	if (addr == 0)
		return (0);

	dst = htonl (addr);
	if (IN_CLASSA (dst))
		return (ntohl (IN_CLASSA_NET));
	if (IN_CLASSB (dst))
		return (ntohl (IN_CLASSB_NET));
	if (IN_CLASSC (dst))
		return (ntohl (IN_CLASSC_NET));

	return (0);
}

char *hwaddr_ntoa (const unsigned char *hwaddr, int hwlen)
{
	static char buffer[(HWADDR_LEN * 3) + 1];
	char *p = buffer;
	int i;

	for (i = 0; i < hwlen && i < HWADDR_LEN; i++) {
		if (i > 0)
			*p ++= ':';
		p += snprintf (p, 3, "%.2x", hwaddr[i]);
	}
		
	*p ++= '\0';

	return (buffer);
}

static int _do_interface (const char *ifname,
						  unsigned char *hwaddr, int *hwlen,
						  struct in_addr *addr,
						  bool flush, bool get)
{
	int s;
	struct ifconf ifc;
	int retval = 0;
	int len = 10 * sizeof (struct ifreq);
	int lastlen = 0;
	char *p;

	if ((s = socket (AF_INET, SOCK_DGRAM, 0)) == -1) {
		logger (LOG_ERR, "socket: %s", strerror (errno));
		return -1;
	}

	/* Not all implementations return the needed buffer size for
	 * SIOGIFCONF so we loop like so for all until it works */
	memset (&ifc, 0, sizeof (struct ifconf));
	while (1) {
		ifc.ifc_len = len;
		ifc.ifc_buf = xmalloc (len);
		if (ioctl (s, SIOCGIFCONF, &ifc) == -1) {
			if (errno != EINVAL || lastlen != 0) {
				logger (LOG_ERR, "ioctl SIOCGIFCONF: %s", strerror (errno));
				close (s);
				free (ifc.ifc_buf);	
				return -1;
			}
		} else {
			if (ifc.ifc_len == lastlen)
				break;
			lastlen = ifc.ifc_len;
		}

		free (ifc.ifc_buf);
		ifc.ifc_buf = NULL;
		len *= 2;
	}

	for (p = ifc.ifc_buf; p < ifc.ifc_buf + ifc.ifc_len;) {
		union {
			char *buffer;
			struct ifreq *ifr;
		} ifreqs;
		struct sockaddr_in address;
		struct ifreq *ifr;

		/* Cast the ifc buffer to an ifreq cleanly */
		ifreqs.buffer = p;
		ifr = ifreqs.ifr;

#ifdef __linux__
		p += sizeof (struct ifreq);
#else
		p += sizeof (ifr->ifr_name) +
			MAX (ifr->ifr_addr.sa_len, sizeof (struct sockaddr));
#endif

		if (strcmp (ifname, ifr->ifr_name) != 0)
			continue;

#ifdef __linux__
		/* Do something with the values at least */
		if (hwaddr && hwlen)
			*hwlen = 0;
#else
		if (hwaddr && hwlen && ifr->ifr_addr.sa_family == AF_LINK) {
			struct sockaddr_dl sdl;

			memcpy (&sdl, &ifr->ifr_addr, sizeof (struct sockaddr_dl));
			*hwlen = sdl.sdl_alen;
			memcpy (hwaddr, sdl.sdl_data + sdl.sdl_nlen, sdl.sdl_alen);
			retval = 1;
			break;
		}
#endif

		if (ifr->ifr_addr.sa_family == AF_INET)	{
			memcpy (&address, &ifr->ifr_addr, sizeof (struct sockaddr_in));
			if (flush) {
				struct sockaddr_in netmask;

				if (ioctl (s, SIOCGIFNETMASK, ifr) == -1) {
					logger (LOG_ERR, "ioctl SIOCGIFNETMASK: %s",
							strerror (errno));
					continue;
				}
				memcpy (&netmask, &ifr->ifr_addr, sizeof (struct sockaddr_in));

				if (del_address (ifname,
								 address.sin_addr, netmask.sin_addr) == -1)
					retval = -1;
			} else if (get) {
				addr->s_addr = address.sin_addr.s_addr;
				retval = 1;
				break;
			} else if (address.sin_addr.s_addr == addr->s_addr) {
				retval = 1;
				break;
			}
		}

	}

	close (s);
	free (ifc.ifc_buf);
	return retval;
}

interface_t *read_interface (const char *ifname, int metric)
{
	int s;
	struct ifreq ifr;
	interface_t *iface;
	unsigned char hwaddr[20];
	int hwlen = 0;
	sa_family_t family = 0;
	unsigned short mtu;
#ifdef __linux__
	char *p;
#endif

	if (! ifname)
		return NULL;

	memset (hwaddr, sizeof (hwaddr), 0);
	memset (&ifr, 0, sizeof (struct ifreq));
	strlcpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));

	if ((s = socket (AF_INET, SOCK_DGRAM, 0)) == -1) {
		logger (LOG_ERR, "socket: %s", strerror (errno));
		return NULL;
	}

#ifdef __linux__
	/* Do something with the metric parameter to satisfy the compiler warning */
	metric = 0;
	strlcpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
	if (ioctl (s, SIOCGIFHWADDR, &ifr) == -1) {
		logger (LOG_ERR, "ioctl SIOCGIFHWADDR: %s", strerror (errno));
		close (s);
		return NULL;
	}

	switch (ifr.ifr_hwaddr.sa_family) {
		case ARPHRD_ETHER:
		case ARPHRD_IEEE802:
			hwlen = ETHER_ADDR_LEN;
			break;
		case ARPHRD_IEEE1394:
			hwlen = EUI64_ADDR_LEN;
		case ARPHRD_INFINIBAND:
			hwlen = INFINIBAND_ADDR_LEN;
			break;
		default:
			logger (LOG_ERR, "interface is not Ethernet, FireWire, InfiniBand or Token Ring");
			close (s);
			return NULL;
	}

	memcpy (hwaddr, ifr.ifr_hwaddr.sa_data, hwlen);
	family = ifr.ifr_hwaddr.sa_family;
#else
	ifr.ifr_metric = metric;
	strlcpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
	if (ioctl (s, SIOCSIFMETRIC, &ifr) == -1) {
		logger (LOG_ERR, "ioctl SIOCSIFMETRIC: %s", strerror (errno));
		close (s);
		return NULL;
	}
	
	if (_do_interface (ifname, hwaddr, &hwlen, NULL, false, false) != 1) {
		logger (LOG_ERR, "could not find interface %s", ifname);
		close (s);
		return NULL;
	}

	family = ARPHRD_ETHER;
#endif

	strlcpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
	if (ioctl (s, SIOCGIFMTU, &ifr) == -1) {
		logger (LOG_ERR, "ioctl SIOCGIFMTU: %s", strerror (errno));
		close (s);
		return NULL;
	}

	if (ifr.ifr_mtu < MTU_MIN) {
		logger (LOG_DEBUG, "MTU of %d is too low, setting to %d", ifr.ifr_mtu, MTU_MIN);
		ifr.ifr_mtu = MTU_MIN;
		strlcpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		if (ioctl (s, SIOCSIFMTU, &ifr) == -1) {
			logger (LOG_ERR, "ioctl SIOCSIFMTU,: %s", strerror (errno));
			close (s);
			return NULL;
		}
	}
	mtu = ifr.ifr_mtu;

	strlcpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
#ifdef __linux__
	/* We can only bring the real interface up */
	if ((p = strchr (ifr.ifr_name, ':')))
		*p = '\0';
#endif
	if (ioctl (s, SIOCGIFFLAGS, &ifr) == -1) {
		logger (LOG_ERR, "ioctl SIOCGIFFLAGS: %s", strerror (errno));
		close (s);
		return NULL;
	}

	if (! (ifr.ifr_flags & IFF_UP) || ! (ifr.ifr_flags & IFF_RUNNING)) {
		ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
		if (ioctl (s, SIOCSIFFLAGS, &ifr) == -1) {
			logger (LOG_ERR, "ioctl SIOCSIFFLAGS: %s", strerror (errno));
			close (s);
			return NULL;
		}
	}

	close (s);

	iface = xmalloc (sizeof (interface_t));
	memset (iface, 0, sizeof (interface_t));
	strlcpy (iface->name, ifname, IF_NAMESIZE);
#ifdef ENABLE_INFO
	snprintf (iface->infofile, PATH_MAX, INFOFILE, ifname);
#endif
	memcpy (&iface->hwaddr, hwaddr, hwlen);
	iface->hwlen = hwlen;

	iface->family = family;
	iface->arpable = ! (ifr.ifr_flags & (IFF_NOARP | IFF_LOOPBACK));
	iface->mtu = iface->previous_mtu = mtu;

	logger (LOG_INFO, "hardware address = %s",
			hwaddr_ntoa (iface->hwaddr, iface->hwlen));

	/* 0 is a valid fd, so init to -1 */
	iface->fd = -1;

	return iface;
}

int get_mtu (const char *ifname)
{
	struct ifreq ifr;
	int r;
	int s;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		logger (LOG_ERR, "socket: %s", strerror (errno));
		return (-1);
	}

	memset (&ifr, 0, sizeof (struct ifreq));
	strlcpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
	r = ioctl (s, SIOCGIFMTU, &ifr);
	close (s);

	if (r == -1) {
		logger (LOG_ERR, "ioctl SIOCGIFMTU: %s", strerror (errno));
		return (-1);
	}

	return (ifr.ifr_mtu);
}

int set_mtu (const char *ifname, short int mtu)
{
	struct ifreq ifr;
	int r;
	int s;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		logger (LOG_ERR, "socket: %s", strerror (errno));
		return (-1);
	}

	memset (&ifr, 0, sizeof (struct ifreq));
	logger (LOG_DEBUG, "setting MTU to %d", mtu);
	strlcpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
	ifr.ifr_mtu = mtu;
	r = ioctl (s, SIOCSIFMTU, &ifr);
	close (s);

	if (r == -1)
		logger (LOG_ERR, "ioctl SIOCSIFMTU: %s", strerror (errno));

	return (r == 0 ? 0 : -1);
}

static void log_route( 
					  struct in_addr destination,
					  struct in_addr netmask,
					  struct in_addr gateway,
					  int metric,
					  int change, int del)
{
	char *dstd = xstrdup (inet_ntoa (destination));

#ifdef __linux__
#define METRIC " metric %d"
#else
#define METRIC ""
	metric = 0;
#endif

	if (gateway.s_addr == destination.s_addr ||
		gateway.s_addr == INADDR_ANY)
		logger (LOG_INFO, "%s route to %s/%d" METRIC,
				change ? "changing" : del ? "removing" : "adding",
				dstd, inet_ntocidr (netmask)
#ifdef __linux__
				, metric
#endif
			   );
	else if (destination.s_addr == INADDR_ANY)
		logger (LOG_INFO, "%s default route via %s" METRIC,
				change ? "changing" : del ? "removing" : "adding",
				inet_ntoa (gateway)

#ifdef __linux__
				, metric
#endif
			   );
	else
		logger (LOG_INFO, "%s route to %s/%d via %s" METRIC,
				change ? "changing" : del ? "removing" : "adding",
				dstd, inet_ntocidr (netmask), inet_ntoa (gateway)
#ifdef __linux__
				, metric
#endif
			   );

	free (dstd);
}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined (__OpenBSD__) \
|| defined(__APPLE__)
static int do_address (const char *ifname, struct in_addr address,
					   struct in_addr netmask, struct in_addr broadcast, int del)
{
	int s;
	struct ifaliasreq ifa;

	if (! ifname)
		return -1;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		logger (LOG_ERR, "socket: %s", strerror (errno));
		return -1;
	}

	memset (&ifa, 0, sizeof (ifa));
	strlcpy (ifa.ifra_name, ifname, sizeof (ifa.ifra_name));

#define ADDADDR(_var, _addr) { \
		union { struct sockaddr *sa; struct sockaddr_in *sin; } _s; \
		_s.sa = &_var; \
		_s.sin->sin_family = AF_INET; \
		_s.sin->sin_len = sizeof (struct sockaddr_in); \
		memcpy (&_s.sin->sin_addr, &_addr, sizeof (struct in_addr)); \
	}

	ADDADDR (ifa.ifra_addr, address);
	ADDADDR (ifa.ifra_mask, netmask);
	if (! del)
		ADDADDR (ifa.ifra_broadaddr, broadcast);

#undef ADDADDR

	if (ioctl (s, del ? SIOCDIFADDR : SIOCAIFADDR, &ifa) == -1) {
		logger (LOG_ERR, "ioctl %s: %s", del ? "SIOCDIFADDR" : "SIOCAIFADDR",
				strerror (errno));
		close (s);
		return -1;
	}

	close (s);
	return 0;
}

static int do_route (const char *ifname,
					 struct in_addr destination,
					 struct in_addr netmask,
					 struct in_addr gateway,
					 int metric,
					 int change, int del)
{
	int s;
	struct rtm 
	{
		struct rt_msghdr hdr;
		char buffer[sizeof (struct sockaddr_storage) * 3];
	} rtm;
	char *bp = rtm.buffer;
	static int seq;
	union sockunion {
		struct sockaddr sa;
		struct sockaddr_in sin;
#ifdef INET6
		struct sockaddr_in6 sin6;
#endif
		struct sockaddr_dl sdl;
		struct sockaddr_storage ss;
	} su;
	
	int l;

	if (! ifname)
		return -1;

	log_route (destination, netmask, gateway, metric, change, del);

	if ((s = socket (PF_ROUTE, SOCK_RAW, 0)) == -1) {
		logger (LOG_ERR, "socket: %s", strerror (errno));
		return -1;
	}

	memset (&rtm, 0, sizeof (struct rtm));

	rtm.hdr.rtm_version = RTM_VERSION;
	rtm.hdr.rtm_seq = ++seq;
	rtm.hdr.rtm_type = change ? RTM_CHANGE : del ? RTM_DELETE : RTM_ADD;
	rtm.hdr.rtm_flags = RTF_UP | RTF_STATIC;

	/* This order is important */
	rtm.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;

#define ADDADDR(_addr) \
	memset (&su, 0, sizeof (struct sockaddr_storage)); \
	su.sin.sin_family = AF_INET; \
	su.sin.sin_len = sizeof (struct sockaddr_in); \
	memcpy (&su.sin.sin_addr, &_addr, sizeof (struct in_addr)); \
	l = SA_SIZE (&(su.sa)); \
	memcpy (bp, &(su), l); \
	bp += l;

	ADDADDR (destination);

	if (netmask.s_addr == INADDR_BROADCAST ||
		gateway.s_addr == INADDR_ANY)
	{
		/* Make us a link layer socket */
		unsigned char hwaddr[HWADDR_LEN];
		int hwlen = 0;

		if (netmask.s_addr == INADDR_BROADCAST) 
			rtm.hdr.rtm_flags |= RTF_HOST;

		_do_interface (ifname, hwaddr, &hwlen, NULL, false, false);
		memset (&su, 0, sizeof (struct sockaddr_storage));
		su.sdl.sdl_len = sizeof (struct sockaddr_dl);
		su.sdl.sdl_family = AF_LINK;
		su.sdl.sdl_nlen = strlen (ifname);
		memcpy (&su.sdl.sdl_data, ifname, su.sdl.sdl_nlen);
		su.sdl.sdl_alen = hwlen;
		memcpy (((unsigned char *) &su.sdl.sdl_data) + su.sdl.sdl_nlen,
				hwaddr, su.sdl.sdl_alen);

		l = SA_SIZE (&(su.sa));
		memcpy (bp, &su, l);
		bp += l;
	} else {
		rtm.hdr.rtm_flags |= RTF_GATEWAY;
		ADDADDR (gateway);
	}

	ADDADDR (netmask);
#undef ADDADDR

	rtm.hdr.rtm_msglen = sizeof (rtm);
	if (write (s, &rtm, sizeof (rtm)) == -1) {
		/* Don't report error about routes already existing */
		if (errno != EEXIST)
			logger (LOG_ERR, "write: %s", strerror (errno));
		close (s);
		return -1;
	}

	close (s);
	return 0;
}

#elif __linux__
/* This netlink stuff is overly compex IMO.
   The BSD implementation is much cleaner and a lot less code.
   send_netlink handles the actual transmission so we can work out
   if there was an error or not.

   As always throughout this code, credit is due :)
   This blatently taken from libnetlink.c from the iproute2 package
   which is the only good source of netlink code.
   */
static int send_netlink(struct nlmsghdr *hdr)
{
	int s;
	pid_t mypid = getpid ();
	struct sockaddr_nl nl;
	struct iovec iov;
	struct msghdr msg;
	static unsigned int seq;
	char buffer[256];
	int bytes;
	union
	{
		char *buffer;
		struct nlmsghdr *nlm;
	} h;

	if ((s = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
		logger (LOG_ERR, "socket: %s", strerror (errno));
		return -1;
	}

	memset (&nl, 0, sizeof (struct sockaddr_nl));
	nl.nl_family = AF_NETLINK;
	if (bind (s, (struct sockaddr *) &nl, sizeof (nl)) == -1) {
		logger (LOG_ERR, "bind: %s", strerror (errno));
		close (s);
		return -1;
	}

	memset (&iov, 0, sizeof (struct iovec));
	iov.iov_base = hdr;
	iov.iov_len = hdr->nlmsg_len;

	memset (&msg, 0, sizeof (struct msghdr));
	msg.msg_name = &nl;
	msg.msg_namelen = sizeof (nl);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* Request a reply */
	hdr->nlmsg_flags |= NLM_F_ACK;
	hdr->nlmsg_seq = ++seq;

	if (sendmsg (s, &msg, 0) == -1) {
		logger (LOG_ERR, "write: %s", strerror (errno));
		close (s);
		return -1;
	}

	memset (buffer, 0, sizeof (buffer));
	iov.iov_base = buffer;

	while (1) {
		iov.iov_len = sizeof (buffer);
		bytes = recvmsg (s, &msg, 0);

		if (bytes == -1) {
			if (errno != EINTR)
				logger (LOG_ERR, "netlink: overrun");
			continue;
		}

		if (bytes == 0) {
			logger (LOG_ERR, "netlink: EOF");
			goto eexit;
		}

		if (msg.msg_namelen != sizeof (nl)) {
			logger (LOG_ERR, "netlink: sender address length mismatch");
			goto eexit;
		}

		for (h.buffer = buffer; bytes >= (signed) sizeof (*h.nlm); ) {
			int len = h.nlm->nlmsg_len;
			int l = len - sizeof (*h.nlm);

			if (l < 0 || len > bytes) {
				if (msg.msg_flags & MSG_TRUNC)
					logger (LOG_ERR, "netlink: truncated message");
				else
					logger (LOG_ERR, "netlink: malformed message");
				goto eexit;
			}

			if (nl.nl_pid != 0 ||
				(pid_t) h.nlm->nlmsg_pid != mypid ||
				h.nlm->nlmsg_seq != seq)
				/* Message isn't for us, so skip it */
				goto next;

			/* We get an NLMSG_ERROR back with a code of zero for success */
			if (h.nlm->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA (h.nlm);
				if ((unsigned) l < sizeof (struct nlmsgerr))
					logger (LOG_ERR, "netlink: truncated error message");
				else {
					errno = -err->error;
					if (errno == 0) {
						close (s);
						return 0;
					}

					/* Don't report on something already existing */
					if (errno != EEXIST)
						logger (LOG_ERR, "netlink: %s", strerror (errno));
				}
				goto eexit;
			}

			logger (LOG_ERR, "netlink: unexpected reply");
next:
			bytes -= NLMSG_ALIGN (len);
			h.buffer += NLMSG_ALIGN (len);
		}

		if (msg.msg_flags & MSG_TRUNC) {
			logger (LOG_ERR, "netlink: truncated message");
			continue;
		}

		if (bytes) {
			logger (LOG_ERR, "netlink: remnant of size %d", bytes);
			goto eexit;
		}
	}

eexit:
	close (s);
	return -1;
}

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((ptrdiff_t) (nmsg)) + NLMSG_ALIGN ((nmsg)->nlmsg_len)))

static int add_attr_l(struct nlmsghdr *n, unsigned int maxlen, int type,
					  const void *data, int alen)
{
	int len = RTA_LENGTH(alen);
	struct rtattr *rta;

	if (NLMSG_ALIGN (n->nlmsg_len) + RTA_ALIGN (len) > maxlen) {
		logger (LOG_ERR, "add_attr_l: message exceeded bound of %d\n", maxlen);
		return -1;
	}

	rta = NLMSG_TAIL (n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy (RTA_DATA (rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN (n->nlmsg_len) + RTA_ALIGN (len);

	return 0;
}

static int add_attr_32(struct nlmsghdr *n, unsigned int maxlen, int type,
					   uint32_t data)
{
	int len = RTA_LENGTH (sizeof (uint32_t));
	struct rtattr *rta;

	if (NLMSG_ALIGN (n->nlmsg_len) + len > maxlen) {
		logger (LOG_ERR, "add_attr32: message exceeded bound of %d\n", maxlen);
		return -1;
	}

	rta = NLMSG_TAIL (n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy (RTA_DATA (rta), &data, sizeof (uint32_t));
	n->nlmsg_len = NLMSG_ALIGN (n->nlmsg_len) + len;

	return 0;
}

static int do_address(const char *ifname,
					  struct in_addr address, struct in_addr netmask,
					  struct in_addr broadcast, int del)
{
	struct
	{
		struct nlmsghdr hdr;
		struct ifaddrmsg ifa;
		char buffer[64];
	}
	nlm;

	if (!ifname)
		return -1;

	memset (&nlm, 0, sizeof (nlm));

	nlm.hdr.nlmsg_len = NLMSG_LENGTH (sizeof (struct ifaddrmsg));
	nlm.hdr.nlmsg_flags = NLM_F_REQUEST;
	if (! del)
		nlm.hdr.nlmsg_flags |= NLM_F_CREATE | NLM_F_REPLACE;
	nlm.hdr.nlmsg_type = del ? RTM_DELADDR : RTM_NEWADDR;
	if (! (nlm.ifa.ifa_index = if_nametoindex (ifname))) {
		logger (LOG_ERR, "if_nametoindex: Couldn't find index for interface `%s'",
				ifname);
		return -1;
	}
	nlm.ifa.ifa_family = AF_INET;

	nlm.ifa.ifa_prefixlen = inet_ntocidr (netmask);

	/* This creates the aliased interface */
	add_attr_l (&nlm.hdr, sizeof (nlm), IFA_LABEL, ifname, strlen (ifname) + 1);

	add_attr_l (&nlm.hdr, sizeof (nlm), IFA_LOCAL, &address.s_addr,
				sizeof (address.s_addr));
	if (! del)
		add_attr_l (&nlm.hdr, sizeof (nlm), IFA_BROADCAST, &broadcast.s_addr,
					sizeof (broadcast.s_addr));

	return send_netlink (&nlm.hdr);
}

static int do_route (const char *ifname,
					 struct in_addr destination,
					 struct in_addr netmask,
					 struct in_addr gateway,
					 int metric, int change, int del)
{
	unsigned int ifindex;
	struct
	{
		struct nlmsghdr hdr;
		struct rtmsg rt;
		char buffer[256];
	}
	nlm;

	if (! ifname)
		return -1;

	log_route (destination, netmask, gateway, metric, change, del);

	memset (&nlm, 0, sizeof (nlm));

	nlm.hdr.nlmsg_len = NLMSG_LENGTH (sizeof (struct rtmsg));
	if (change)
		nlm.hdr.nlmsg_flags = NLM_F_REPLACE;
	else if (! del)
		nlm.hdr.nlmsg_flags = NLM_F_CREATE | NLM_F_EXCL;
	nlm.hdr.nlmsg_flags |= NLM_F_REQUEST;
	nlm.hdr.nlmsg_type = del ? RTM_DELROUTE : RTM_NEWROUTE;
	nlm.rt.rtm_family = AF_INET;
	nlm.rt.rtm_table = RT_TABLE_MAIN;

	if (del)
		nlm.rt.rtm_scope = RT_SCOPE_NOWHERE;
	else {
		nlm.hdr.nlmsg_flags |= NLM_F_CREATE | NLM_F_EXCL;
		nlm.rt.rtm_protocol = RTPROT_BOOT;
		if (netmask.s_addr == INADDR_BROADCAST ||
			gateway.s_addr == INADDR_ANY)
			nlm.rt.rtm_scope = RT_SCOPE_LINK;
		else
			nlm.rt.rtm_scope = RT_SCOPE_UNIVERSE;
		nlm.rt.rtm_type = RTN_UNICAST;
	}

	nlm.rt.rtm_dst_len = inet_ntocidr (netmask);
	add_attr_l (&nlm.hdr, sizeof (nlm), RTA_DST, &destination.s_addr,
				sizeof (destination.s_addr));
	if (netmask.s_addr != INADDR_BROADCAST &&
		destination.s_addr != gateway.s_addr)
		add_attr_l (&nlm.hdr, sizeof (nlm), RTA_GATEWAY, &gateway.s_addr,
					sizeof (gateway.s_addr));

	if (! (ifindex = if_nametoindex (ifname))) {
		logger (LOG_ERR, "if_nametoindex: Couldn't find index for interface `%s'",
				ifname);
		return -1;
	}

	add_attr_32 (&nlm.hdr, sizeof (nlm), RTA_OIF, ifindex);
	add_attr_32 (&nlm.hdr, sizeof (nlm), RTA_PRIORITY, metric);

	return send_netlink (&nlm.hdr);
}

#else
#error "Platform not supported!"
#error "We currently support BPF and Linux sockets."
#error "Other platforms may work using BPF. If yours does, please let me know"
#error "so I can add it to our list."
#endif


int add_address (const char *ifname, struct in_addr address,
				 struct in_addr netmask, struct in_addr broadcast)
{
	logger (LOG_INFO, "adding IP address %s/%d",
			inet_ntoa (address), inet_ntocidr (netmask));

	return (do_address (ifname, address, netmask, broadcast, 0));
}

int del_address (const char *ifname,
				 struct in_addr address, struct in_addr netmask)
{
	struct in_addr t;

	logger (LOG_INFO, "deleting IP address %s/%d",
			inet_ntoa (address), inet_ntocidr (netmask));

	memset (&t, 0, sizeof (t));
	return (do_address (ifname, address, netmask, t, 1));
}

int add_route (const char *ifname, struct in_addr destination,
			   struct in_addr netmask, struct in_addr gateway, int metric)
{
	return (do_route (ifname, destination, netmask, gateway, metric, 0, 0));
}

int change_route (const char *ifname, struct in_addr destination,
				  struct in_addr netmask, struct in_addr gateway, int metric)
{
	return (do_route (ifname, destination, netmask, gateway, metric, 1, 0));
}

int del_route (const char *ifname, struct in_addr destination,
			   struct in_addr netmask, struct in_addr gateway, int metric)
{
	return (do_route (ifname, destination, netmask, gateway, metric, 0, 1));
}


int flush_addresses (const char *ifname)
{
	return (_do_interface (ifname, NULL, NULL, NULL, true, false));
}

unsigned long get_address (const char *ifname)
{
	struct in_addr address;
	if (_do_interface (ifname, NULL, NULL, &address, false, true) > 0)
		return (address.s_addr);
	return (0);
}

int has_address (const char *ifname, struct in_addr address)
{
	return (_do_interface (ifname, NULL, NULL, &address, false, false));
}
