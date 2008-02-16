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

#ifndef DHCP_H
#define DHCP_H

#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include "dhcpcd.h"
#include "interface.h"


/* Max MTU - defines dhcp option length */
#define MTU_MAX             1500
#define MTU_MIN             576

/* UDP port numbers for DHCP */
#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68

#define MAGIC_COOKIE        0x63825363
#define BROADCAST_FLAG      0x8000

/* DHCP message OP code */
#define DHCP_BOOTREQUEST    1
#define DHCP_BOOTREPLY      2

/* DHCP message type */
#define DHCP_DISCOVER       1
#define DHCP_OFFER          2
#define DHCP_REQUEST        3
#define DHCP_DECLINE        4
#define DHCP_ACK            5
#define DHCP_NAK            6
#define DHCP_RELEASE        7
#define DHCP_INFORM         8

/* DHCP options */
enum DHCP_OPTIONS
{
	DHCP_PAD                    = 0,
	DHCP_NETMASK                = 1,
	DHCP_TIMEROFFSET            = 2,
	DHCP_ROUTERS                = 3,
	DHCP_TIMESERVER             = 4,
	DHCP_NAMESERVER             = 5,
	DHCP_DNSSERVER              = 6,
	DHCP_LOGSERVER              = 7,
	DHCP_COOKIESERVER           = 8,
	DHCP_HOSTNAME               = 12,
	DHCP_DNSDOMAIN              = 15,
	DHCP_ROOTPATH               = 17,
	DHCP_DEFAULTIPTTL           = 23,
	DHCP_MTU                    = 26,
	DHCP_BROADCAST              = 28,
	DHCP_MASKDISCOVERY          = 29,
	DHCP_ROUTERDISCOVERY        = 31,
	DHCP_STATICROUTE            = 33,
	DHCP_NISDOMAIN              = 40,
	DHCP_NISSERVER              = 41,
	DHCP_NTPSERVER              = 42,
	DHCP_ADDRESS                = 50,
	DHCP_LEASETIME              = 51,
	DHCP_OPTIONSOVERLOADED      = 52,
	DHCP_MESSAGETYPE            = 53,
	DHCP_SERVERIDENTIFIER       = 54,
	DHCP_PARAMETERREQUESTLIST   = 55,
	DHCP_MESSAGE                = 56,
	DHCP_MAXMESSAGESIZE         = 57,
	DHCP_RENEWALTIME            = 58,
	DHCP_REBINDTIME             = 59,
	DHCP_CLASSID                = 60,
	DHCP_CLIENTID               = 61,
	DHCP_USERCLASS              = 77,  /* RFC 3004 */
	DHCP_FQDN                   = 81,
	DHCP_DNSSEARCH              = 119, /* RFC 3397 */
	DHCP_SIPSERVER              = 120, /* RFC 3361 */
	DHCP_CSR                    = 121, /* RFC 3442 */
	DHCP_END                    = 255
};

/* SetFQDNHostName values - lsnybble used in flags
 * byte (see buildmsg.c), hsnybble to create order
 * and to allow 0x00 to mean disable
 */
enum FQQN {
	FQDN_DISABLE    = 0x00,
	FQDN_NONE       = 0x18,
	FQDN_PTR        = 0x20,
	FQDN_BOTH       = 0x31
};

typedef struct fqdn_t
{
	uint8_t flags;
	uint8_t r1;
	uint8_t r2;
	char *name;
} fqdn_t;

typedef struct dhcp_t
{
	char version[11];

	struct in_addr serveraddress;
	char serverhw[IF_NAMESIZE];
	char servername[64];

	struct in_addr address;
	struct in_addr netmask;
	struct in_addr broadcast;
	unsigned short mtu;

	unsigned int leasedfrom;
	unsigned int leasetime;
	unsigned int renewaltime;
	unsigned int rebindtime;

	route_t *routes;

	char *hostname;
	fqdn_t *fqdn;

	address_t *dnsservers;
	char *dnsdomain;
	char *dnssearch;

	address_t *ntpservers;

	address_t *nisservers;
	char *nisdomain;

	char *sipservers;

	char *message;
	char *rootpath;

	bool frominfo;
} dhcp_t;



/* Sizes for DHCP options */
#define DHCP_CHADDR_LEN         16
#define SERVERNAME_LEN          64
#define BOOTFILE_LEN            128
#define DHCP_UDP_LEN            (20 + 8)
#define DHCP_BASE_LEN           (4 + 4 + 2 + 2 + 4 + 4 + 4 + 4 + 4)
#define DHCP_RESERVE_LEN        (4 + 4 + 4 + 4 + 2)
#define DHCP_FIXED_LEN          (DHCP_BASE_LEN + DHCP_CHADDR_LEN + \
                                 + SERVERNAME_LEN + BOOTFILE_LEN)
#define DHCP_OPTION_LEN         (MTU_MAX - DHCP_FIXED_LEN - DHCP_UDP_LEN \
                                 - DHCP_RESERVE_LEN)

/* Some crappy DHCP servers require the BOOTP minimum length */
#define BOOTP_MESSAGE_LENTH_MIN 300

typedef struct dhcpmessage_t
{
	unsigned char op;           /* message type */
	unsigned char hwtype;       /* hardware address type */
	unsigned char hwlen;        /* hardware address length */
	unsigned char hwopcount;        /* should be zero in client message */
	int32_t xid;            /* transaction id */
	int16_t secs;           /* elapsed time in sec. from boot */
	int16_t flags;
	int32_t ciaddr;         /* (previously allocated) client IP */
	int32_t yiaddr;         /* 'your' client IP address */
	int32_t siaddr;         /* should be zero in client's messages */
	int32_t giaddr;         /* should be zero in client's messages */
	unsigned char chaddr[DHCP_CHADDR_LEN];  /* client's hardware address */
	unsigned char servername[SERVERNAME_LEN];    /* server host name */
	unsigned char bootfile[BOOTFILE_LEN];    /* boot file name */
	uint32_t cookie;
	unsigned char options[DHCP_OPTION_LEN]; /* message options - cookie */
} dhcpmessage_t;

struct udp_dhcp_packet
{
	struct ip ip;
	struct udphdr udp;
	dhcpmessage_t dhcp;
};

size_t send_message (const interface_t *iface, const dhcp_t *dhcp,
					 unsigned long xid, char type,
					 const options_t *options);
void free_dhcp (dhcp_t *dhcp);
int parse_dhcpmessage (dhcp_t *dhcp, const dhcpmessage_t *message);

#endif
