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

#include <sys/stat.h>

#include <arpa/inet.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "interface.h"
#include "logger.h"
#include "info.h"

#ifdef ENABLE_INFO
static char *cleanmetas (const char *cstr)
{
	/* The largest single element we can have is 256 bytes according to the RFC,
	   so this buffer size should be safe even if it's all ' */
	static char buffer[1024]; 
	char *b = buffer;

	memset (buffer, 0, sizeof (buffer));
	if (cstr == NULL || strlen (cstr) == 0)
		return b;

	do
		if (*cstr == 39) {
			*b++ = '\'';
			*b++ = '\\';
			*b++ = '\'';
			*b++ = '\'';
		} else
			*b++ = *cstr;
	while (*cstr++);

	*b++ = 0;
	b = buffer;

	return b;
}

bool write_info(const interface_t *iface, const dhcp_t *dhcp,
				const options_t *options, bool overwrite)
{
	FILE *f;
	route_t *route;
	address_t *address;
	struct stat sb;

	if (options->test)
		f = stdout;
	else {
		if (! overwrite && stat (iface->infofile, &sb) == 0)
			return (true);

		logger (LOG_DEBUG, "writing %s", iface->infofile);
		if ((f = fopen (iface->infofile, "w")) == NULL) {
			logger (LOG_ERR, "fopen `%s': %s", iface->infofile, strerror (errno));
			return (false);
		}
	}

	if (dhcp->address.s_addr) {
		fprintf (f, "IPADDR='%s'\n", inet_ntoa (dhcp->address));
		fprintf (f, "NETMASK='%s'\n", inet_ntoa (dhcp->netmask));
		fprintf (f, "BROADCAST='%s'\n", inet_ntoa (dhcp->broadcast));
	}
	if (dhcp->mtu > 0)
		fprintf (f, "MTU='%d'\n", dhcp->mtu);

	if (dhcp->routes) {
		bool doneone = false;
		fprintf (f, "ROUTES='");
		for (route = dhcp->routes; route; route = route->next) {
			if (route->destination.s_addr != 0) {
				if (doneone)
					fprintf (f, " ");
				fprintf (f, "%s", inet_ntoa (route->destination));
				fprintf (f, ",%s", inet_ntoa (route->netmask));
				fprintf (f, ",%s", inet_ntoa (route->gateway));
				doneone = true;
			}
		}
		fprintf (f, "'\n");

		doneone = false;
		fprintf (f, "GATEWAYS='");
		for (route = dhcp->routes; route; route = route->next) {
			if (route->destination.s_addr == 0) {
				if (doneone)
					fprintf (f, " ");
				fprintf (f, "%s", inet_ntoa (route->gateway));
				doneone = true;
			}
		}
		fprintf (f, "'\n");
	}

	if (dhcp->hostname)
		fprintf (f, "HOSTNAME='%s'\n", cleanmetas (dhcp->hostname));

	if (dhcp->dnsdomain)
		fprintf (f, "DNSDOMAIN='%s'\n", cleanmetas (dhcp->dnsdomain));

	if (dhcp->dnssearch)
		fprintf (f, "DNSSEARCH='%s'\n", cleanmetas (dhcp->dnssearch));

	if (dhcp->dnsservers) {
		fprintf (f, "DNSSERVERS='");
		for (address = dhcp->dnsservers; address; address = address->next) {
			fprintf (f, "%s", inet_ntoa (address->address));
			if (address->next)
				fprintf (f, " ");
		}
		fprintf (f, "'\n");
	}

	if (dhcp->fqdn) {
		fprintf (f, "FQDNFLAGS='%u'\n", dhcp->fqdn->flags);
		fprintf (f, "FQDNRCODE1='%u'\n", dhcp->fqdn->r1);
		fprintf (f, "FQDNRCODE2='%u'\n", dhcp->fqdn->r2);
		fprintf (f, "FQDNHOSTNAME='%s'\n", dhcp->fqdn->name);
	}

	if (dhcp->ntpservers) {
		fprintf (f, "NTPSERVERS='");
		for (address = dhcp->ntpservers; address; address = address->next) {
			fprintf (f, "%s", inet_ntoa (address->address));
			if (address->next)
				fprintf (f, " ");
		}
		fprintf (f, "'\n");
	}

	if (dhcp->nisdomain)
		fprintf (f, "NISDOMAIN='%s'\n", cleanmetas (dhcp->nisdomain));

	if (dhcp->nisservers) {
		fprintf (f, "NISSERVERS='");
		for (address = dhcp->nisservers; address; address = address->next) {
			fprintf (f, "%s", inet_ntoa (address->address));
			if (address->next)
				fprintf (f, " ");
		}
		fprintf (f, "'\n");
	}

	if (dhcp->rootpath)
		fprintf (f, "ROOTPATH='%s'\n", cleanmetas (dhcp->rootpath));

	if (dhcp->sipservers)
		fprintf (f, "SIPSERVERS='%s'\n", cleanmetas (dhcp->sipservers));

	if (dhcp->serveraddress.s_addr)
		fprintf (f, "DHCPSID='%s'\n", inet_ntoa (dhcp->serveraddress));
	if (dhcp->servername[0])
		fprintf (f, "DHCPSNAME='%s'\n", cleanmetas (dhcp->servername));
	if (! options->doinform && dhcp->address.s_addr) {
		if (! options->test)
			fprintf (f, "LEASEDFROM='%u'\n", dhcp->leasedfrom);
		fprintf (f, "LEASETIME='%u'\n", dhcp->leasetime);
		fprintf (f, "RENEWALTIME='%u'\n", dhcp->renewaltime);
		fprintf (f, "REBINDTIME='%u'\n", dhcp->rebindtime);
	}
	fprintf (f, "INTERFACE='%s'\n", iface->name);
	fprintf (f, "CLASSID='%s'\n", cleanmetas (options->classid));
	if (options->clientid_len > 0)
		fprintf (f, "CLIENTID='00:%s'\n", cleanmetas (options->clientid));
#ifdef ENABLE_DUID
	else if (iface->duid_length > 0 && options->clientid_len != -1) {
		unsigned char duid[256];
		unsigned char *p = duid;
		uint32_t ul;

		*p++ = 255;

		/* IAID is 4 bytes, so if the interface name is 4 bytes then use it */
		if (strlen (iface->name) == 4) {
			memcpy (p, iface->name, 4);
		} else {
			/* Name isn't 4 bytes, so use the index */
			ul = htonl (if_nametoindex (iface->name));
			memcpy (p, &ul, 4);
		}
		p += 4;

		memcpy (p, iface->duid, iface->duid_length);
		p += iface->duid_length;

		fprintf (f, "CLIENTID='%s'\n", hwaddr_ntoa (duid, p - duid));
	}
#endif
	else
		fprintf (f, "CLIENTID='%.2X:%s'\n", iface->family,
				 hwaddr_ntoa (iface->hwaddr, iface->hwlen));
	fprintf (f, "DHCPCHADDR='%s'\n", hwaddr_ntoa (iface->hwaddr, iface->hwlen));

#ifdef ENABLE_INFO_COMPAT
	/* Support the old .info settings if we need to */
	fprintf (f, "\n# dhcpcd-1.x and 2.x compatible variables\n");
	if (dhcp->dnsservers) {
		fprintf (f, "DNS='");
		for (address = dhcp->dnsservers; address; address = address->next) {
			fprintf (f, "%s", inet_ntoa (address->address));
			if (address->next)
				fprintf (f, ",");
		}
		fprintf (f, "'\n");
	}

	if (dhcp->routes) {
		bool doneone = false;
		fprintf (f, "GATEWAY='");
		for (route = dhcp->routes; route; route = route->next) {
			if (route->destination.s_addr == 0) {
				if (doneone)
					fprintf (f, ",");
				fprintf (f, "%s", inet_ntoa (route->gateway));
				doneone = true;
			}
		}
		fprintf (f, "'\n");
	}
#endif

	if (! options->test)
		fclose (f);
	return (true);
}

static bool parse_address (struct in_addr *addr,
						   const char *value, const char *var)
{
	if (inet_aton (value, addr) == 0) {
		logger (LOG_ERR, "%s `%s': %s", var, value,
				strerror (errno));
		return (false);
	}
	return (true);
}

static bool parse_uint (unsigned int *i,
						const char *value, const char *var)
{
	if (sscanf (value, "%u", i) != 1) {
		logger (LOG_ERR, "%s `%s': not a valid number",
				var, value);
		return (false);
	}
	return (true);
}

static bool parse_ushort (unsigned short *s,
						  const char *value, const char *var)
{
	if (sscanf (value, "%hu", s) != 1) {
		logger (LOG_ERR, "%s `%s': not a valid number",
				var, value);
		return (false);
	}
	return (true);
}

static bool parse_addresses (address_t **address, char *value, const char *var)
{
	char *token;
	char *p = value;
	bool retval = true;

	while ((token = strsep (&p, " "))) {
		address_t *a = xmalloc (sizeof (address_t));
		memset (a, 0, sizeof (address_t));

		if (inet_aton (token, &a->address) == 0) {
			logger (LOG_ERR, "%s: invalid address `%s'", var, token);
			free (a);
			retval = false;
		} else {
			if (*address) {
				address_t *aa = *address;
				while (aa->next)
					aa = aa->next;
				aa->next = a;
			} else
				*address = a;
		}
	}

	return (retval);
}

bool read_info (const interface_t *iface, dhcp_t *dhcp)
{
	FILE *fp;
	char buffer[1024];
	char *var;
	char *value;
	char *p;
	struct stat sb;

	if (stat (iface->infofile, &sb) != 0) {
		logger (LOG_ERR, "lease information file `%s' does not exist",
				iface->infofile);
		return (false);
	}

	if (! (fp = fopen (iface->infofile, "r"))) {
		logger (LOG_ERR, "fopen `%s': %s", iface->infofile, strerror (errno));
		return (false);
	}

	dhcp->frominfo = true;

	memset (buffer, 0, sizeof (buffer));
	while ((fgets (buffer, sizeof (buffer), fp))) {
		var = buffer;

		/* Strip leading spaces/tabs */
		while ((*var == ' ') || (*var == '\t'))
			var++;

		/* Trim trailing \n */
		p = var + strlen (var) - 1;
		if (*p == '\n')
			*p = 0;


		/* Skip comments */
		if (*var == '#')
			continue;
		
		/* If we don't have an equals sign then skip it */
		if (! (p = strchr (var, '=')))
			continue;

		/* Terminate the = so we have two strings */
		*p = 0;

		value = p + 1;
		/* Strip leading and trailing quotes if present */
		if (*value == '\'' || *value == '"')
			value++;
		p = value + strlen (value) - 1;
		if (*p == '\'' || *p == '"')
			*p = 0;

		/* Don't process null vars or values */
		if (! *var || ! *value)
			continue;

		if (strcmp (var, "IPADDR") == 0)
			parse_address (&dhcp->address, value, "IPADDR");
		else if (strcmp (var, "NETMASK") == 0)
			parse_address (&dhcp->netmask, value, "NETMASK");
		else if (strcmp (var, "BROADCAST") == 0)
			parse_address (&dhcp->broadcast, value, "BROADCAST");
		else if (strcmp (var, "MTU") == 0)
			parse_ushort (&dhcp->mtu, value, "MTU");
		else if (strcmp (var, "ROUTES") == 0) {
			p = value;
			while ((value = strsep (&p, " "))) {
				char *pp = value;
				char *dest = strsep (&pp, ",");
				char *net = strsep (&pp, ",");
				char *gate = strsep (&pp, ",");
				route_t *route;

				if (! dest || ! net || ! gate) {
					logger (LOG_ERR, "read_info ROUTES `%s,%s,%s': invalid route",
							dest, net, gate);
					continue;
				}

				/* See if we can create a route */
				route = xmalloc (sizeof (route_t));
				memset (route, 0, sizeof (route_t));
				if (inet_aton (dest, &route->destination) == 0) {
					logger (LOG_ERR, "read_info ROUTES `%s': not a valid destination address",
							dest);
					free (route);
					continue;
				}
				if (inet_aton (dest, &route->netmask) == 0) {
					logger (LOG_ERR, "read_info ROUTES `%s': not a valid netmask address",
							net);
					free (route);
					continue;
				}
				if (inet_aton (dest, &route->gateway) == 0) {
					logger (LOG_ERR, "read_info ROUTES `%s': not a valid gateway address",
							gate);
					free (route);
					continue;
				}

				/* OK, now add our route */
				if (dhcp->routes) {
					route_t *r = dhcp->routes;
					while (r->next)
						r = r->next;
					r->next = route;
				} else
					dhcp->routes = route;
			}
		} else if (strcmp (var, "GATEWAYS") == 0) {
			p = value;
			while ((value = strsep (&p, " "))) {
				route_t *route = xmalloc (sizeof (route_t));
				memset (route, 0, sizeof (route_t));
				if (parse_address (&route->gateway, value, "GATEWAYS")) {
					if (dhcp->routes) {
						route_t *r = dhcp->routes;
						while (r->next)
							r = r->next;
						r->next = route;
					} else
						dhcp->routes = route;
				} else
					free (route);
			}
		} else if (strcmp (var, "HOSTNAME") == 0)
			dhcp->hostname = xstrdup (value);
		else if (strcmp (var, "DNSDOMAIN") == 0)
			dhcp->dnsdomain = xstrdup (value);
		else if (strcmp (var, "DNSSEARCH") == 0)
			dhcp->dnssearch = xstrdup (value);
		else if (strcmp (var, "DNSSERVERS") == 0)
			parse_addresses (&dhcp->dnsservers, value, "DNSSERVERS");
		else if (strcmp (var, "NTPSERVERS") == 0)
			parse_addresses (&dhcp->ntpservers, value, "NTPSERVERS");
		else if (strcmp (var, "NISDOMAIN") == 0)
			dhcp->nisdomain = xstrdup (value);
		else if (strcmp (var, "NISSERVERS") == 0)
			parse_addresses (&dhcp->nisservers, value, "NISSERVERS");
		else if (strcmp (var, "ROOTPATH") == 0)
			dhcp->rootpath = xstrdup (value);
		else if (strcmp (var, "DHCPSID") == 0)
			parse_address (&dhcp->serveraddress, value, "DHCPSID");
		else if (strcmp (var, "DHCPSNAME") == 0)
			strlcpy (dhcp->servername, value, sizeof (dhcp->servername));
		else if (strcmp (var, "LEASEDFROM") == 0)
			parse_uint (&dhcp->leasedfrom, value, "LEASEDFROM");
		else if (strcmp (var, "LEASETIME") == 0)
			parse_uint (&dhcp->leasetime, value, "LEASETIME");
		else if (strcmp (var, "RENEWALTIME") == 0)
			parse_uint (&dhcp->renewaltime, value, "RENEWALTIME");
		else if (strcmp (var, "REBINDTIME") == 0)
			parse_uint (&dhcp->rebindtime, value, "REBINDTIME");
	}

	fclose (fp);
	return (true);
}

#endif

