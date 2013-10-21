/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2013  Mike Shal <marfey@gmail.com>
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

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
/* _WIN32_IE needed for SHGetSpecialFolderPath() */
#define _WIN32_IE 0x0500
#include <windows.h>
#include <shlobj.h>
#endif

#include "compat.h"
#include "option.h"
#include "vardb.h"
#include "inih/ini.h"
#include "init.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/ioctl.h>
#endif

#define OPT_MAX 256

static char home_loc[PATH_MAX];

static struct config_file {
	const char *file;
	struct vardb root;
	int exists;
} static_configs[] = {
	{ .file = TUP_OPTIONS_FILE },
	{ .file = home_loc },
#ifndef _WIN32
	{ .file = "/etc/tup/options" },
#endif
};
#define NUM_OPTION_LOCATIONS (sizeof(static_configs) / sizeof(static_configs[0]))

static struct dynamic_config_file_list {
	struct dynamic_config_file_list *next;
	struct config_file dynamic_config;
} *dynamic_configs = NULL;

static int parse_option_file(int x);
static void parse_ini_file(FILE *f);
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
	{"updater.full_deps", "0", NULL},
	{"updater.warnings", "1", NULL},
	{"display.color", "auto", NULL},
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
	{"graph.combine", "0", NULL},
};
#define NUM_OPTIONS (sizeof(options) / sizeof(options[0]))

static int inited = 0;
static volatile sig_atomic_t win_resize_requested = 0;

#ifdef _WIN32
static void reset_console(void);
static CONSOLE_SCREEN_BUFFER_INFO csbi;
#else
static void win_resize_handler(int sig);
static struct sigaction sigact = {
	.sa_handler = win_resize_handler,
	.sa_flags = SA_RESTART,
};
#endif

int tup_option_process_ini(void) {
	int cur_dir;
	int best_root = -1; // file descriptor -> best root candidate
	int found_tup_dir = 0;

	cur_dir = open(".", 0);
	if (cur_dir < 0) {
		perror("open(\".\", 0)");
		fprintf(stderr, "tup error: Could not get reference to current directory?\n");
		exit(1);
	}

	for(;;) {
		FILE *f;
		struct stat st;
		char path_buf[8];

		f = fopen("Tupfile.ini", "r");
		if (f == NULL) {
			if (errno != ENOENT) {
				perror("fopen");
				fprintf(stderr, "tup error: Unexpected error opening ini file\n");
				exit(1);
			}
		} else {
			parse_ini_file(f);

			if (best_root != -1)
				close(best_root);
			/* open can never fail as we have
			   already read a file from this dir */
			best_root = open(".", 0);
			assert(best_root >= 0);

			fclose(f);
		}

		if (stat(".tup", &st) == 0 && S_ISDIR(st.st_mode)) {
			found_tup_dir = 1;
			break;
		}

		if (chdir("..")) {
			perror("chdir");
			fprintf(stderr, "tup error: Unexpected error traversing directory tree\n");
			exit(1);
		}

		if (NULL == getcwd(path_buf, sizeof(path_buf))) {
			if (errno != ERANGE) {
				perror("getcwd");
				fprintf(stderr, "tup error: Unexpected error getting directory while traversing the tree\n");
			}
		} else {
			const char root_path[] = {PATH_SEP, '\0'};
			if (0 == strcmp(root_path, path_buf)) {
				break;
			}
		}
	}

	if (best_root == -1) {
		goto ini_cleanup;
	}

	if (!found_tup_dir) {
		int rc;
		int argc = 1;
		char argv0[] = "init";
		char *argv[] = {argv0, NULL};
		char root_path[PATH_MAX];

		if (fchdir(best_root) < 0) {
			perror("fchdir(best_root)");
			fprintf(stderr, "tup error: Could not chdir to root candidate?\n");
			exit(1);
		}

		if (NULL == getcwd(root_path, sizeof(root_path))) {
			if (errno != ERANGE) {
				perror("getcwd");
				fprintf(stderr, "tup error: Unexpected error getting root path\n");
				exit(1);
			}
			printf("Initilizing .tup directory\n");
		} else {
			printf("Initilizing .tup in %s\n", root_path);
		}

		rc = init_command(argc, argv);
		if (0 != rc) {
			fprintf(stderr, "tup error: `tup init' failed unexpectedly\n");
			exit(rc);
		}
	}

	if(close(best_root) < 0) {
		perror("close(best_root");
	}

ini_cleanup:
	if (fchdir(cur_dir) < 0) {
		perror("fchdir(cur_dir)");
		fprintf(stderr, "tup error: Could not chdir back to original working directory?\n");
		exit(1);
	}
	if (close(cur_dir) < 0) {
		perror("close");
		fprintf(stderr, "tup error: Unexpected error closing current directory file descriptor\n");
		exit(1);
	}
	return 0;
}

int tup_option_init(void)
{
	unsigned int x;

#ifdef _WIN32
	if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
		atexit(reset_console);
#endif

	for(x=0; x<NUM_OPTIONS; x++) {
		if(options[x].generator)
			options[x].default_value = options[x].generator();
	}

	if(init_home_loc() < 0)
		return -1;

	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		if(vardb_init(&static_configs[x].root) < 0)
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
	struct dynamic_config_file_list *c = dynamic_configs;
	unsigned int x;

	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		vardb_close(&static_configs[x].root);
	}

	while (c != NULL) {
		struct dynamic_config_file_list *temp = c;

		free((char *) c->dynamic_config.file);
		c = c->next;
		free(temp);
	}
	dynamic_configs = NULL;
}

static int tup_boolify_flag(const char *value) {
	if(strcasecmp(value, "true") == 0)
		return 1;
	if(strcasecmp(value, "yes") == 0)
		return 1;
	if(strcasecmp(value, "false") == 0)
		return 0;
	if(strcasecmp(value, "no") == 0)
		return 0;
	return atoi(value);
}

int tup_option_get_flag(const char *opt)
{
	const char *value = tup_option_get_string(opt);
	return tup_boolify_flag(value);
}

int tup_option_get_int(const char *opt)
{
	return atoi(tup_option_get_string(opt));
}

const char *tup_option_get_string(const char *opt)
{
	unsigned int x;
	int len = strlen(opt);
	struct dynamic_config_file_list *c = dynamic_configs;

	if(!inited) {
		fprintf(stderr, "tup internal error: Called tup_option_get_string(%s) before the options were initialized.\n", opt);
		exit(1);
	}

	while(c != NULL) {
		struct var_entry *ve;
		ve = vardb_get(&c->dynamic_config.root, opt, len);
		if(ve) {
			return ve->value;
		}
		c = c->next;
	}

	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		struct var_entry *ve;
		ve = vardb_get(&static_configs[x].root, opt, len);
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

const char *tup_option_get_location(const char *opt)
{
	unsigned int x;
	int len = strlen(opt);
	if(!inited) {
		fprintf(stderr, "tup internal error: Called tup_option_get_location(%s) before the options were initialized.\n", opt);
		exit(1);
	}
	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		struct var_entry *ve;
		ve = vardb_get(&static_configs[x].root, opt, len);
		if(ve) {
			return static_configs[x].file;
		}
	}
	return "compiled-in defaults";
}

int tup_option_show(void)
{
	unsigned int x;
	printf(" --- Option files:\n");
	for(x=0; x<NUM_OPTION_LOCATIONS; x++) {
		if(static_configs[x].exists) {
			printf("Parsed option file: %s\n", static_configs[x].file);
		} else {
			printf("Option file does not exist: %s\n", static_configs[x].file);
		}
	}
	printf(" --- Option settings:\n");
	for(x=0; x<NUM_OPTIONS; x++) {
		const char *value = options[x].default_value;
		const char *location = "default values";
		int len = strlen(options[x].name);
		int found_option = 0;

		struct dynamic_config_file_list *c = dynamic_configs;
		unsigned int y;

		while (!found_option && (c != NULL)) {
			struct var_entry *ve;
			ve = vardb_get(&c->dynamic_config.root, options[x].name, len);
			if(ve) {
				location = c->dynamic_config.file;
				value = ve->value;
				found_option = 1;
			}
			c = c->next;
		}

		y = 0;
		while (!found_option && (y < NUM_OPTION_LOCATIONS)) {
			struct var_entry *ve;
			ve = vardb_get(&static_configs[y].root, options[x].name, len);
			if(ve) {
				location = static_configs[y].file;
				value = ve->value;
				found_option = 1;
			}
			y++;
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

	fprintf(stderr, "tup warning: Option '%s' in section '%s' is unknown to this version of tup. It will be ignored.", name, section);
	return 1; /* inih success, keep going and ignore unknowns to future-proof */

set_var:
	if(vardb_set(vdb, opt, value, NULL) < 0)
		return 0; /* inih error code */
	return 1; /* inih success */
}

static int parse_option_file(int x)
{
	FILE *f;
	int rc;

	f = fopen(static_configs[x].file, "r");
	if(!f) {
		/* Don't care if the file's not there or we can't read it */
		static_configs[x].exists = 0;
		return 0;
	}
	static_configs[x].exists = 1;
	rc = ini_parse_file(f, parse_callback, &static_configs[x].root);
	fclose(f);
	if(rc == 0)
		return 0;
	return -1;
}

static void parse_ini_file(FILE *f) {
	struct dynamic_config_file_list *c = dynamic_configs;
	int rc;
	const char* ini_name = "Tupfile.ini";
	/* strlen("Tupfile.ini") == 11. Some compilers don't grok const ints */
	char file_path[PATH_MAX + 1 + 11] = {0};

	if (c == NULL) {
		dynamic_configs = malloc(sizeof(struct dynamic_config_file_list));
		c = dynamic_configs;
	} else {
		while (c->next != NULL)
			c = c->next;
		c->next = malloc(sizeof(struct dynamic_config_file_list));
		c = c->next;
	}
	assert(c && "malloc config struct");
	c->next = NULL;

	if (NULL == getcwd(file_path, PATH_MAX)) {
		perror("getcwd");
		fprintf(stderr, "tup error: Unexpected error getting current directory\n");
		exit(1);
	}
	file_path[strlen(file_path)] = PATH_SEP; // note: still \0-terminated from array init
	strcpy(file_path+strlen(file_path), ini_name);
	c->dynamic_config.file = strdup(file_path);
	if (c->dynamic_config.file == NULL) {
		perror("strdup");
		fprintf(stderr, "tup error: Unexpected error allocating memory for file path\n");
		exit(1);
	}

	if(vardb_init(&c->dynamic_config.root) < 0) {
		fprintf(stderr, "tup internal error: Failed to initialize vardb for .ini config entry\n");
		exit(1);
	}

	rc = ini_parse_file(f, parse_callback, &c->dynamic_config.root);

	if (rc != 0) {
		fprintf(stderr, "tup error: Failed to parse ini file\n");
		exit(1);
	}
}

static const char *cpu_number(void)
{
	static char buf[10];

	int count = 1;
#if defined(__linux__) || defined(__sun__) || defined(__FreeBSD__)
	count = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__APPLE__)
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


#ifdef _WIN32
static void reset_console(void)
{
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), csbi.wAttributes);
}
#else
static void win_resize_handler(int sig)
{
	if(sig) {}
	win_resize_requested = 1;
}
#endif
