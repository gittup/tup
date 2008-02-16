/*
 * dhcpcd - DHCP client daemon -
 * Copyright 2005 - 2007 Roy Marples <uberlord@gentoo.org>
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

#ifndef ARP_H
#define ARP_H

#ifdef ENABLE_ARP
#include <netinet/in.h>

#include "interface.h"

int arp_claim (interface_t *iface, struct in_addr address);
#endif

#endif
