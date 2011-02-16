/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "option.h"
#include "vardb.h"
#include "compat.h"
#include "inih/ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#define OPT_MAX 256

static const char *locations[] = {
	"project settings",
	"user settings",
	"system settings",
};
#define NUM_OPTION_LOCATIONS (sizeof(locations) / sizeof(locations[0]))

static const char *cpu_number(void);
static int parse_option_file(const char *file, struct vardb *vdb);

static struct option {
	const char *name;
	const char *default_value;
	/* function that generates default value dynamically */
	const char *(*generator)(void);
} options[] = {
	{"updater.num_jobs", NULL, cpu_number},
	{"updater.keep_going", "0", NULL},
	{"display.color", "auto", NULL},
	{"monitor.autoupdate", "0", NULL},
	{"monitor.foreground", "0", NULL},
	{"db.sync", "1", NULL},
};
#define NUM_OPTIONS (sizeof(options) / sizeof(options[0]))

static struct vardb roots[NUM_OPTION_LOCATIONS];
static int inited = 0;

int tup_option_init(void)
{
	unsigned int x;
	char homefile[PATH_MAX];
	const char *home;

	for(x=0; x<NUM_OPTIONS; x++) {
		if(options[x].generator)
			options[x].default_value = options[x].generator();
	}

	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		if(vardb_init(&roots[x]) < 0)
			return -1;
	}
	if(parse_option_file(TUP_OPTIONS_FILE, &roots[0]) < 0)
		return -1;
	home = getenv("HOME");
	if(home) {
		if(snprintf(homefile, sizeof(homefile), "%s/%s", home, ".tupoptions") >= (signed)sizeof(homefile)) {
			fprintf(stderr, "tup internal error: user-level options file string is too small.\n");
			return -1;
		}
		if(parse_option_file(homefile, &roots[1]) < 0)
			return -1;
	}
	if(parse_option_file("/etc/tup/options", &roots[2]) < 0)
		return -1;
	inited = 1;
	return 0;
}

void tup_option_exit(void)
{
	unsigned int x;
	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		vardb_close(&roots[x]);
	}
}

int tup_option_get_flag(const char *opt)
{
	const char *value = tup_option_get_string(opt);

	if(strcmp(value, "true") == 0)
		return 1;
	if(strcmp(value, "yes") == 0)
		return 1;
	if(strcmp(value, "false") == 0)
		return 0;
	if(strcmp(value, "no") == 0)
		return 0;
	return atoi(value);
}

int tup_option_get_int(const char *opt)
{
	return atoi(tup_option_get_string(opt));
}

const char *tup_option_get_string(const char *opt)
{
	unsigned int x;
	int len = strlen(opt);
	if(!inited) {
		fprintf(stderr, "tup internal error: Called tup_option_get_string(%s) before the options were initialized.\n", opt);
		exit(1);
	}
	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		struct var_entry *ve;
		ve = vardb_get(&roots[x], opt, len);
		if(ve) {
			return ve->value;
		}
	}

	for(x=0; x<NUM_OPTIONS; x++) {
		if(strcmp(opt, options[x].name) == 0)
			return options[x].default_value;
	}
	fprintf(stderr, "tup internal error: Option '%s' does not have a default value\n", opt);
	exit(1);
}

int tup_option_show(void)
{
	unsigned int x;
	for(x=0; x<NUM_OPTIONS; x++) {
		unsigned int y;
		const char *value = options[x].default_value;
		const char *location = "default values";
		int len = strlen(options[x].name);

		for(y=0; y<NUM_OPTION_LOCATIONS; y++) {
			struct var_entry *ve;
			ve = vardb_get(&roots[y], options[x].name, len);
			if(ve) {
				location = locations[y];
				value = ve->value;
				break;
			}
		}
		printf("%s = '%s' from %s\n", options[x].name, value, location);
	}
	return 0;
}

static int parse_callback(void *user, const char *section, const char *name,
			  const char *value)
{
	struct vardb *vdb = user;
	char opt[OPT_MAX];
	unsigned int x;

	if(snprintf(opt, sizeof(opt), "%s.%s", section, name) >= (signed)sizeof(opt)) {
		fprintf(stderr, "tup error: Option '%s.%s' is too long.\n", section, name);
		return 0; /* inih error code */
	}
	for(x=0; x<NUM_OPTIONS; x++) {
		if(strcmp(options[x].name, opt) == 0)
			goto set_var;
	}
	fprintf(stderr, "tup error: Option '%s' in section '%s' does not correspond to a valid option. Please set one of the following:\n\n", name, section);
	tup_option_show();
	fprintf(stderr, "\nSee the tup.1 man page for details on how to properly set the options.\n");
	return 0; /* inih error code */

set_var:
	if(vardb_set(vdb, opt, value, NULL) < 0)
		return 0; /* inih error code */
	return 1; /* inih success */
}

static int parse_option_file(const char *file, struct vardb *vdb)
{
	FILE *f;
	int rc;

	f = fopen(file, "r");
	if(!f) {
		/* Don't care if the file's not there or we can't read it */
		return 0;
	}
	rc = ini_parse_file(f, parse_callback, vdb);
	fclose(f);
	if(rc == 0)
		return 0;
	return -1;
}

static const char *cpu_number(void)
{
	static char buf[10];

	int count = 1;
#if defined(__linux__) || defined(__sun__)
	count = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__APPLE__) || defined(__FreeBSD__)
	int nm[2];
	size_t len = 4;

	nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
	sysctl(nm, 2, &count, &len, NULL, 0);

	if(count < 1) {
		nm[1] = HW_NCPU;
		sysctl(nm, 2, &count, &len, NULL, 0);
		if(count < 1) { count = 1; }
	}
#elif defined(_WIN32)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	count = sysinfo.dwNumberOfProcessors;
#endif

	if (count > 100000 || count < 0)
		count = 1;

	snprintf(buf, sizeof(buf), "%d", count);
	return buf;
}
