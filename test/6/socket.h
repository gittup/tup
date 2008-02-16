/*
 * dhcpcd - DHCP client daemon -
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

#ifndef SOCKET_H
#define SOCKET_H

#include <stdbool.h>

#include "dhcp.h"
#include "interface.h"

void make_dhcp_packet(struct udp_dhcp_packet *packet,
					  const unsigned char *data, int length,
					  struct in_addr source, struct in_addr dest);

int open_socket (interface_t *iface, bool arp);
int send_packet (const interface_t *iface, int type,
				 const unsigned char *data, int len);
int get_packet (const interface_t *iface, unsigned char *data,
				unsigned char *buffer, int *buffer_len, int *buffer_pos);
#endif
