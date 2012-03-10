/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2012  Mike Shal <marfey@gmail.com>
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

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
/* _WIN32_IE needed for SHGetSpecialFolderPath() */
#define _WIN32_IE 0x0400
#include <windows.h>
#include <shlobj.h>
#endif

#include "option.h"
#include "vardb.h"
#include "inih/ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/ioctl.h>
#endif

#define OPT_MAX 256

static char home_loc[PATH_MAX];

static struct {
	const char *file;
	struct vardb root;
	int exists;
} locations[] = {
	{ .file = TUP_OPTIONS_FILE },
	{ .file = home_loc },
#ifndef _WIN32
	{ .file = "/etc/tup/options" },
#endif
};
#define NUM_OPTION_LOCATIONS (sizeof(locations) / sizeof(locations[0]))

#ifdef _WIN32
#define DEFAULT_COLOR "never"
#else
#define DEFAULT_COLOR "auto"
#endif

static int parse_option_file(int x);
static const char *cpu_number(void);
static const char *stdout_isatty(void);
static const char *get_console_width(void);
static int init_home_loc(void);

static struct option {
	const char *name;
	const char *default_value;
	/* function that generates default value dynamically */
	const char *(*generator)(void);
} options[] = {
	{"updater.num_jobs", NULL, cpu_number},
	{"updater.keep_going", "0", NULL},
	{"display.color", DEFAULT_COLOR, NULL},
	{"display.width", NULL, get_console_width},
	{"display.progress", NULL, stdout_isatty},
	{"display.job_numbers", "1", NULL},
	{"display.job_time", "1", NULL},
	{"monitor.autoupdate", "0", NULL},
	{"monitor.autoparse", "0", NULL},
	{"monitor.foreground", "0", NULL},
	{"db.sync", "1", NULL},
	{"graph.dirs", "0", NULL},
	{"graph.ghosts", "0", NULL},
	{"graph.environment", "0", NULL},
};
#define NUM_OPTIONS (sizeof(options) / sizeof(options[0]))

static int inited = 0;
static volatile sig_atomic_t win_resize_requested = 0;

#ifndef _WIN32
static void win_resize_handler(int sig);
static struct sigaction sigact = {
	.sa_handler = win_resize_handler,
	.sa_flags = SA_RESTART,
};
#endif

int tup_option_init(void)
{
	unsigned int x;

	for(x=0; x<NUM_OPTIONS; x++) {
		if(options[x].generator)
			options[x].default_value = options[x].generator();
	}

	if(init_home_loc() < 0)
		return -1;

	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		if(vardb_init(&locations[x].root) < 0)
			return -1;
		if(parse_option_file(x) < 0)
			return -1;
	}
#ifndef _WIN32
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGWINCH, &sigact, NULL);
#endif
	inited = 1;
	return 0;
}

void tup_option_exit(void)
{
	unsigned int x;
	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		vardb_close(&locations[x].root);
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
		ve = vardb_get(&locations[x].root, opt, len);
		if(ve) {
			return ve->value;
		}
	}

	for(x=0; x<NUM_OPTIONS; x++) {
		if(strcmp(opt, options[x].name) == 0) {
			if(win_resize_requested &&
			   strcmp(opt, "display.width") == 0) {
				options[x].default_value = get_console_width();
			}
			return options[x].default_value;
		}
	}
	fprintf(stderr, "tup internal error: Option '%s' does not have a default value\n", opt);
	exit(1);
}

int tup_option_show(void)
{
	unsigned int x;
	printf(" --- Option files:\n");
	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		if(locations[x].exists) {
			printf("Parsed option file: %s\n", locations[x].file);
		} else {
			printf("Option file does not exist: %s\n", locations[x].file);
		}
	}
	printf(" --- Option settings:\n");
	for(x=0; x<NUM_OPTIONS; x++) {
		unsigned int y;
		const char *value = options[x].default_value;
		const char *location = "default values";
		int len = strlen(options[x].name);

		for(y=0; y<NUM_OPTION_LOCATIONS; y++) {
			struct var_entry *ve;
			ve = vardb_get(&locations[y].root, options[x].name, len);
			if(ve) {
				location = locations[y].file;
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

static int parse_option_file(int x)
{
	FILE *f;
	int rc;

	f = fopen(locations[x].file, "r");
	if(!f) {
		/* Don't care if the file's not there or we can't read it */
		locations[x].exists = 0;
		return 0;
	}
	locations[x].exists = 1;
	rc = ini_parse_file(f, parse_callback, &locations[x].root);
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
	buf[sizeof(buf) - 1] = 0;
	return buf;
}

static const char *get_console_width(void)
{
	static char buf[10];
	int width = 0;
#ifdef TIOCGWINSZ
	struct winsize wsz;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz) == 0)
		width = wsz.ws_col;
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
		width = csbi.dwSize.X;
#endif
	snprintf(buf, sizeof(buf), "%d", width);
	buf[sizeof(buf) - 1] = 0;
	return buf;
}

static const char *stdout_isatty(void)
{
	static char buf[10];

	snprintf(buf, sizeof(buf), "%d", isatty(STDOUT_FILENO));
	buf[sizeof(buf) - 1]= 0;
	return buf;
}

static int init_home_loc(void)
{
#if defined(_WIN32)
	char folderpath[MAX_PATH];
	if(!SHGetSpecialFolderPath(NULL, folderpath, CSIDL_COMMON_APPDATA, 0)) {
		fprintf(stderr, "tup error: Unable to get Application Data path.\n");
		return -1;
	}
	if(snprintf(home_loc, sizeof(home_loc), "%s\\tup\\tup.ini", folderpath) >= (signed)sizeof(home_loc)) {
			fprintf(stderr, "tup internal error: user-level options file string is too small.\n");
			return -1;
	}
	return 0;
#else
	const char *home;

	home = getenv("HOME");
	if(home) {
		if(snprintf(home_loc, sizeof(home_loc), "%s/.tupoptions", home) >= (signed)sizeof(home_loc)) {
			fprintf(stderr, "tup internal error: user-level options file string is too small.\n");
			return -1;
		}
	}
	return 0;
#endif
}

#ifndef _WIN32
static void win_resize_handler(int sig)
{
	if(sig) {}
	win_resize_requested = 1;
}
#endif
