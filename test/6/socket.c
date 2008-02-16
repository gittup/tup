/*
 * Copyright 2006-2007 Roy Marples <uberlord@gentoo.org>
 * although a lot was lifted from udhcp
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
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#ifndef __OpenBSD__
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#endif
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dhcp.h"
#include "interface.h"
#include "logger.h"
#include "socket.h"

/* A suitably large buffer for all transactions.
   BPF buffer size is set by the kernel, so no define. */
#ifdef __linux__
#define BUFFER_LENGTH 4096
#endif

static uint16_t checksum (unsigned char *addr, uint16_t len)
{
	uint32_t sum = 0;
	union
	{
		unsigned char *addr;
		uint16_t *i;
	} p;
	uint16_t nleft = len;

	p.addr = addr;
	while (nleft > 1) {
		sum += *p.i++;
		nleft -= 2;
	}


	if (nleft == 1) {
		uint8_t a = 0;
		memcpy (&a, p.i, 1);
		sum += ntohs (a) << 8;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return ~sum;
}

void make_dhcp_packet(struct udp_dhcp_packet *packet,
					  const unsigned char *data, int length,
					  struct in_addr source, struct in_addr dest)
{
	struct ip *ip = &packet->ip;
	struct udphdr *udp = &packet->udp;

	/* OK, this is important :)
	   We copy the data to our packet and then create a small part of the
	   ip structure and an invalid ip_len (basically udp length).
	   We then fill the udp structure and put the checksum
	   of the whole packet into the udp checksum.
	   Finally we complete the ip structure and ip checksum.
	   If we don't do the ordering like so then the udp checksum will be
	   broken, so find another way of doing it! */

	memcpy (&packet->dhcp, data, length);

	ip->ip_p = IPPROTO_UDP;
	ip->ip_src.s_addr = source.s_addr;
	if (dest.s_addr == 0)
		ip->ip_dst.s_addr = INADDR_BROADCAST;
	else
		ip->ip_dst.s_addr = dest.s_addr;

	udp->uh_sport = htons (DHCP_CLIENT_PORT);
	udp->uh_dport = htons (DHCP_SERVER_PORT);
	udp->uh_ulen = htons (sizeof (struct udphdr) + length);
	ip->ip_len = udp->uh_ulen;
	udp->uh_sum = checksum ((unsigned char *) packet,
							sizeof (struct udp_dhcp_packet));

	ip->ip_v = IPVERSION;
	ip->ip_hl = 5;
	ip->ip_id = 0;
	ip->ip_tos = IPTOS_LOWDELAY;
	ip->ip_len = htons (sizeof (struct ip) + sizeof (struct udphdr) +
						length);
	ip->ip_id = 0;
	ip->ip_off = 0;
	ip->ip_ttl = IPDEFTTL;

	ip->ip_sum = checksum ((unsigned char *) ip, sizeof (struct ip));
}

static int valid_dhcp_packet (unsigned char *data)
{
	union
	{
		unsigned char *data;
		struct udp_dhcp_packet *packet;
	} d;
	uint16_t bytes;
	uint16_t ipsum;
	uint16_t iplen;
	uint16_t udpsum;
	struct in_addr source;
	struct in_addr dest;
	int retval = 0;

	d.data = data;
	bytes = ntohs (d.packet->ip.ip_len);
	ipsum = d.packet->ip.ip_sum;
	iplen = d.packet->ip.ip_len;
	udpsum = d.packet->udp.uh_sum;

	d.data = data;
	d.packet->ip.ip_sum = 0;
	if (ipsum != checksum ((unsigned char *) &d.packet->ip,
						   sizeof (struct ip)))
	{
		logger (LOG_DEBUG, "bad IP header checksum, ignoring");
		retval = -1;
		goto eexit;
	}

	memcpy (&source, &d.packet->ip.ip_src, sizeof (struct in_addr));
	memcpy (&dest, &d.packet->ip.ip_dst, sizeof (struct in_addr));
	memset (&d.packet->ip, 0, sizeof (struct ip));
	d.packet->udp.uh_sum = 0;

	d.packet->ip.ip_p = IPPROTO_UDP;
	memcpy (&d.packet->ip.ip_src, &source, sizeof (struct in_addr));
	memcpy (&d.packet->ip.ip_dst, &dest, sizeof (struct in_addr));
	d.packet->ip.ip_len = d.packet->udp.uh_ulen;
	if (udpsum && udpsum != checksum (d.data, bytes)) {
		logger (LOG_ERR, "bad UDP checksum, ignoring");
		retval = -1;
	}

eexit:
	d.packet->ip.ip_sum = ipsum;
	d.packet->ip.ip_len = iplen;
	d.packet->udp.uh_sum = udpsum;

	return retval;
}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined (__OpenBSD__) \
	|| defined(__APPLE__)
/* Credit where credit is due :)
   The below BPF filter is taken from ISC DHCP */

# include <net/bpf.h>

static struct bpf_insn dhcp_bpf_filter [] = {
	/* Make sure this is an IP packet... */
	BPF_STMT (BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_IP, 0, 8),

	/* Make sure it's a UDP packet... */
	BPF_STMT (BPF_LD + BPF_B + BPF_ABS, 23),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, IPPROTO_UDP, 0, 6),

	/* Make sure this isn't a fragment... */
	BPF_STMT (BPF_LD + BPF_H + BPF_ABS, 20),
	BPF_JUMP (BPF_JMP + BPF_JSET + BPF_K, 0x1fff, 4, 0),

	/* Get the IP header length... */
	BPF_STMT (BPF_LDX + BPF_B + BPF_MSH, 14),

	/* Make sure it's to the right port... */
	BPF_STMT (BPF_LD + BPF_H + BPF_IND, 16),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, DHCP_CLIENT_PORT, 0, 1),

	/* If we passed all the tests, ask for the whole packet. */
	BPF_STMT (BPF_RET+BPF_K, (u_int) - 1),

	/* Otherwise, drop it. */
	BPF_STMT (BPF_RET+BPF_K, 0),
};

static struct bpf_insn arp_bpf_filter [] = {
	/* Make sure this is an ARP packet... */
	BPF_STMT (BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_ARP, 0, 3),

	/* Make sure this is an ARP REPLY... */
	BPF_STMT (BPF_LD + BPF_H + BPF_ABS, 20),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, ARPOP_REPLY, 0, 1),

	/* If we passed all the tests, ask for the whole packet. */
	BPF_STMT (BPF_RET+BPF_K, (u_int) - 1),

	/* Otherwise, drop it. */
	BPF_STMT (BPF_RET+BPF_K, 0),
};

int open_socket (interface_t *iface, bool arp)
{
	int n = 0;
	int fd = -1;
	char device[PATH_MAX];
	int flags;
	struct ifreq ifr;
	int buf = 0;
	struct bpf_program p;

	do {
		snprintf (device, PATH_MAX, "/dev/bpf%d",  n++);
		fd = open (device, O_RDWR);
	} while (fd == -1 && errno == EBUSY);

	if (fd == -1) {
		logger (LOG_ERR, "unable to open a BPF device");
		return -1;
	}

	if ((flags = fcntl (fd, F_GETFD, 0)) == -1
		|| fcntl (fd, F_SETFD, flags | FD_CLOEXEC) == -1)
	{
		logger (LOG_ERR, "fcntl: %s", strerror (errno));
		close (fd);
		return -1;
	}

	memset (&ifr, 0, sizeof (struct ifreq));
	strlcpy (ifr.ifr_name, iface->name, sizeof (ifr.ifr_name));
	if (ioctl (fd, BIOCSETIF, &ifr) == -1) {
		logger (LOG_ERR, "cannot attach interface `%s' to bpf device `%s': %s",
				iface->name, device, strerror (errno));
		close (fd);
		return -1;
	}

	/* Get the required BPF buffer length from the kernel. */
	if (ioctl (fd, BIOCGBLEN, &buf) == -1) {
		logger (LOG_ERR, "ioctl BIOCGBLEN: %s", strerror (errno));
		close (fd);
		return -1;
	}
	iface->buffer_length = buf;

	flags = 1;
	if (ioctl (fd, BIOCIMMEDIATE, &flags) == -1) {
		logger (LOG_ERR, "ioctl BIOCIMMEDIATE: %s", strerror (errno));
		close (fd);
		return -1;
	}

	/* Install the DHCP filter */
	if (arp) {
		p.bf_insns = arp_bpf_filter;
		p.bf_len = sizeof (arp_bpf_filter) / sizeof (struct bpf_insn);
	} else {
		p.bf_insns = dhcp_bpf_filter;
		p.bf_len = sizeof (dhcp_bpf_filter) / sizeof (struct bpf_insn);
	}
	if (ioctl (fd, BIOCSETF, &p) == -1) {
		logger (LOG_ERR, "ioctl BIOCSETF: %s", strerror (errno));
		close (fd);
		return -1;
	}

	if (iface->fd > -1)
		close (iface->fd);
	iface->fd = fd;

	return fd;
}

int send_packet (const interface_t *iface, int type,
				 const unsigned char *data, int len)
{
	int retval = -1;
	struct iovec iov[2];

	if (iface->family == ARPHRD_ETHER) {
		struct ether_header hw;
		memset (&hw, 0, sizeof (struct ether_header));
		memset (&hw.ether_dhost, 0xff, ETHER_ADDR_LEN);
		hw.ether_type = htons (type);

		iov[0].iov_base = &hw;
		iov[0].iov_len = sizeof (struct ether_header);
	} else {
		logger (LOG_ERR, "unsupported interace type %d", iface->family);
		return -1;
	}
	iov[1].iov_base = (unsigned char *) data;
	iov[1].iov_len = len;

	if ((retval = writev(iface->fd, iov, 2)) == -1)
		logger (LOG_ERR, "writev: %s", strerror (errno));

	return retval;
}

/* BPF requires that we read the entire buffer.
   So we pass the buffer in the API so we can loop on >1 dhcp packet. */
int get_packet (const interface_t *iface, unsigned char *data,
				unsigned char *buffer, int *buffer_len, int *buffer_pos)
{
	union
	{
		unsigned char *buffer;
		struct bpf_hdr *packet;
	} bpf;

	bpf.buffer = buffer;

	if (*buffer_pos < 1) {
		memset (bpf.buffer, 0, iface->buffer_length);
		*buffer_len = read (iface->fd, bpf.buffer, iface->buffer_length);
		*buffer_pos = 0;
		if (*buffer_len < 1) {
			struct timeval tv;
			logger (LOG_ERR, "read: %s", strerror (errno));
			tv.tv_sec = 3;
			tv.tv_usec = 0;
			select (0, NULL, NULL, NULL, &tv);
			return -1;
		}
	} else
		bpf.buffer += *buffer_pos;

	while (bpf.packet) {
		int len = -1;
		union
		{
			unsigned char *buffer;
			struct ether_header *hw;
		} hdr;
		unsigned char *payload;

		/* Ensure that the entire packet is in our buffer */
		if (*buffer_pos + bpf.packet->bh_hdrlen + bpf.packet->bh_caplen
			> (unsigned) *buffer_len)
			break;

		hdr.buffer = bpf.buffer + bpf.packet->bh_hdrlen;
		payload = hdr.buffer + sizeof (struct ether_header);

		/* If it's an ARP reply, then just send it back */
		if (hdr.hw->ether_type == htons (ETHERTYPE_ARP)) {
			len = bpf.packet->bh_caplen - sizeof (struct ether_header);
			memcpy (data, payload, len);
		} else {
			if (valid_dhcp_packet (payload) >= 0) {
				union
				{
					unsigned char *buffer;
					struct udp_dhcp_packet *packet;
				} pay;
				pay.buffer = payload;
				len = ntohs (pay.packet->ip.ip_len) - sizeof (struct ip) -
					sizeof (struct udphdr);
				memcpy (data, &pay.packet->dhcp, len);
			}
		}

		/* Update the buffer_pos pointer */
		bpf.buffer +=
			BPF_WORDALIGN (bpf.packet->bh_hdrlen + bpf.packet->bh_caplen);
		if (bpf.buffer - buffer <  *buffer_len)
			*buffer_pos = bpf.buffer - buffer;
		else
			*buffer_pos = 0;

		if (len != -1)
			return len;

		if (*buffer_pos == 0)
			break;
	}

	/* No valid packets left, so return */
	*buffer_pos = 0;
	return -1;
}

#elif __linux__

#include <netpacket/packet.h>

int open_socket (interface_t *iface, bool arp)
{
	int fd;
	int flags;
	union sockunion {
		struct sockaddr sa;
		struct sockaddr_ll sll;
		struct sockaddr_storage ss;
	} su;

	if ((fd = socket (PF_PACKET, SOCK_DGRAM, htons (ETH_P_IP))) == -1) {
		logger (LOG_ERR, "socket: %s", strerror (errno));
		return -1;
	}

	if ((flags = fcntl (fd, F_GETFD, 0)) == -1
		|| fcntl (fd, F_SETFD, flags | FD_CLOEXEC) == -1)
	{
		logger (LOG_ERR, "fcntl: %s", strerror (errno));
		close (fd);
		return -1;
	}

	memset (&su, 0, sizeof (struct sockaddr_storage));
	su.sll.sll_family = AF_PACKET;
	if (arp)
		su.sll.sll_protocol = htons (ETH_P_ARP);
	else
		su.sll.sll_protocol = htons (ETH_P_IP);
	if (! (su.sll.sll_ifindex = if_nametoindex (iface->name))) {
		logger (LOG_ERR, "if_nametoindex: Couldn't find index for interface `%s'",
				iface->name);
		close (fd);
		return -1;
	}

	if (bind (fd, &su.sa, sizeof (struct sockaddr_storage)) == -1)
	{
		logger (LOG_ERR, "bind: %s", strerror (errno));
		close (fd);
		return -1;
	}

	if (iface->fd > -1)
		close (iface->fd);
	iface->fd = fd;
	iface->socket_protocol = ntohs (su.sll.sll_protocol);

	iface->buffer_length = BUFFER_LENGTH;

	return fd;
}

int send_packet (const interface_t *iface, const int type,
				 const unsigned char *data, const int len)
{
	union sockunion {
		struct sockaddr sa;
		struct sockaddr_ll sll;
		struct sockaddr_storage ss;
	} su;
	int retval;

	if (! iface)
		return -1;

	memset (&su, 0, sizeof (struct sockaddr_storage));
	su.sll.sll_family = AF_PACKET;
	su.sll.sll_protocol = htons (type);
	if (! (su.sll.sll_ifindex = if_nametoindex (iface->name))) {
		logger (LOG_ERR, "if_nametoindex: Couldn't find index for interface `%s'",
				iface->name);
		return -1;
	}
	su.sll.sll_halen = iface->hwlen;
	memset(&su.sll.sll_addr, 0xff, iface->hwlen);

	if ((retval = sendto (iface->fd, data, len, 0, &su.sa,
						  sizeof (struct sockaddr_storage))) == -1)

		logger (LOG_ERR, "sendto: %s", strerror (errno));
	return retval;
}

/* Linux has no need for the buffer as we can read as much as we want.
   We only have the buffer listed to keep the same API. */
int get_packet (const interface_t *iface, unsigned char *data,
				unsigned char *buffer, int *buffer_len, int *buffer_pos)
{
	long bytes;
	union
	{
		unsigned char *buffer;
		struct udp_dhcp_packet *packet;
	} pay;

	/* We don't use the given buffer, but we need to rewind the position */
	*buffer_pos = 0;

	memset (buffer, 0, iface->buffer_length);
	bytes = read (iface->fd, buffer, iface->buffer_length);

	if (bytes == -1) {
		struct timeval tv;
		logger (LOG_ERR, "read: %s", strerror (errno));
		tv.tv_sec = 3;
		tv.tv_usec = 0;
		select (0, NULL, NULL, NULL, &tv);
		return -1;
	}

	*buffer_len = bytes;
	/* If it's an ARP reply, then just send it back */
	if (iface->socket_protocol == ETH_P_ARP) {
		memcpy (data, buffer, bytes);
		return bytes;
	}

	if ((unsigned) bytes < (sizeof (struct ip) + sizeof (struct udphdr))) {
		logger (LOG_DEBUG, "message too short, ignoring");
		return -1;
	}

	pay.buffer = buffer;
	if (bytes < ntohs (pay.packet->ip.ip_len)) {
		logger (LOG_DEBUG, "truncated packet, ignoring");
		return -1;
	}

	bytes = ntohs (pay.packet->ip.ip_len);

	/* This is like our BPF filter above */
	if (pay.packet->ip.ip_p != IPPROTO_UDP || pay.packet->ip.ip_v != IPVERSION ||
		pay.packet->ip.ip_hl != sizeof (pay.packet->ip) >> 2 ||
		pay.packet->udp.uh_dport != htons (DHCP_CLIENT_PORT) ||
		bytes > (int) sizeof (struct udp_dhcp_packet) ||
		ntohs (pay.packet->udp.uh_ulen)
		!= (uint16_t) (bytes - sizeof (pay.packet->ip)))
	{
		return -1;
	}

	if (valid_dhcp_packet (buffer) == -1)
		return -1;

	memcpy(data, &pay.packet->dhcp,
		   bytes - (sizeof (pay.packet->ip) + sizeof (pay.packet->udp)));

	return bytes - (sizeof (pay.packet->ip) + sizeof (pay.packet->udp));
}

#else
#error "Platform not supported!"
#error "We currently support BPF and Linux sockets."
#error "Other platforms may work using BPF. If yours does, please let me know"
#error "so I can add it to our list."
#endif
