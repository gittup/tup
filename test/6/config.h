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

#ifndef CONFIG_H
#define CONFIG_H

/* You can enable/disable various chunks of optional code here.
 * You would only do this to try and shrink the end binary if dhcpcd
 * was running on a low memory device */

#define ENABLE_ARP
#define ENABLE_NTP
#define ENABLE_NIS
#define ENABLE_INFO
/* Define this to enable some compatability with 1.x and 2.x info files */
// #define ENABLE_INFO_COMPAT

/* IPV4LL, aka ZeroConf, aka APIPA, aka RFC 3927.
 * Needs ARP. */
#define ENABLE_IPV4LL

/* We will auto create a DUID_LLT file if it doesn't exist.
 * You can always create your own DUID file that just contains the
 * hex string that represents the DUID.
 * See RFC 3315 for details on this. */
#define ENABLE_DUID

/* Packname name and pathname definitions.
 * NOTE: The service restart commands are Gentoo specific and will
 * probably need to be adapted for your OS. */

#define PACKAGE             "dhcpcd"

#define RESOLVCONF          "/sbin/resolvconf"

#define ETCDIR              "/etc"
#define RESOLVFILE          ETCDIR "/resolv.conf"

#define NISFILE             ETCDIR "/yp.conf"
#define NISSERVICE          ETCDIR "/init.d/ypbind"
#define NISRESTARTARGS      "--nodeps", "--quiet", "conditionalrestart"

#define NTPFILE             ETCDIR "/ntp.conf"
#define NTPDRIFTFILE        ETCDIR "/ntp.drift"
#define NTPLOGFILE          "/var/log/ntp.log"
#define NTPSERVICE          ETCDIR "/init.d/ntpd"
#define NTPRESTARTARGS      "--nodeps", "--quiet", "conditionalrestart"

#define OPENNTPFILE         ETCDIR "/ntpd.conf"
#define OPENNTPSERVICE      ETCDIR "/init.d/ntpd"
#define OPENNTPRESTARTARGS  "--nodeps", "--quiet", "conditionalrestart"

#define DEFAULT_SCRIPT      ETCDIR "/" PACKAGE ".sh"

#define STATEDIR            "/var"
#define PIDFILE             STATEDIR "/run/" PACKAGE "-%s.pid"

#define CONFIGDIR           STATEDIR "/lib/" PACKAGE
#define INFOFILE            CONFIGDIR "/" PACKAGE "-%s.info"

#define DUIDFILE            CONFIGDIR "/" PACKAGE ".duid"

#endif
