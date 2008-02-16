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
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "common.h"
#include "duid.h"
#include "logger.h"

#ifdef ENABLE_DUID

#define THIRTY_YEARS_IN_SECONDS    946707779

void get_duid (interface_t *iface)
{
	FILE *fp;
	uint16_t type = 0;
	uint16_t hw = 0;
	uint32_t ul;
	time_t t;
	unsigned char *p = iface->duid;
	int x = 0;

	if (! iface)
		return;

	/* Remove any existing */
	iface->duid[0] = '\0';
	iface->duid_length = 0;

	/* If we already have a DUID then use it as it's never supposed
	 * to change once we have one even if the interfaces do */
	if ((fp = fopen (DUIDFILE, "r"))) {
		char *fduid;
		char *fdp;
		fduid = fdp = xmalloc ((sizeof (char *) * DUID_LEN * 2) + 1);
		if (fscanf (fp, "%260s", fduid) == 1) {
			char c[3];
			c[2] = '\0';
			while (*fdp) {
				c[0] = *fdp++;
				c[1] = *fdp++;
				*p++ = (char) strtol (c, NULL, 16);
			}
		}
		free (fduid);
		iface->duid_length = p - iface->duid;
		fclose (fp);
		return;
	}

	if (errno != ENOENT) {
		logger (LOG_ERR, "fopen `%s': %s", DUIDFILE, strerror (errno));
		return;
	}

	/* No file? OK, lets make one based on our interface */
	type = htons (1); /* DUI-D-LLT */
	memcpy (p, &type, 2);
	p += 2;

	hw = htons (iface->family);
	memcpy (p, &hw, 2);
	p += 2;

	/* time returns seconds from jan 1 1970, but DUID-LLT is
	 * seconds from jan 1 2000 modulo 2^32 */
	t = time (NULL) - THIRTY_YEARS_IN_SECONDS;
	ul = htonl (t & 0xffffffff);
	memcpy (p, &ul, 4);
	p += 4;

	/* Finally, add the MAC address of the interface */
	memcpy (p, iface->hwaddr, iface->hwlen);
	p += iface->hwlen;
	
	iface->duid_length = p - iface->duid;

	if (! (fp = fopen (DUIDFILE, "w")))
		logger (LOG_ERR, "fopen `%s': %s", DUIDFILE, strerror (errno));
	else {
		int i;
		for (i = 0; i < iface->duid_length; i++)
			x += fprintf (fp, "%.2X", iface->duid[i]);
		fprintf (fp, "\n");
		fclose (fp);
	}

	/* Failed to write the duid? scrub it, we cannot use it */
	if (x < 1) {
		memset (iface->duid, 0, sizeof (iface->duid));
		iface->duid_length = 0;
	}
}
#endif
