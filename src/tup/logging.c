/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2018  Mike Shal <marfey@gmail.com>
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

#include "logging.h"
#include "graph.h"
#include "entry.h"
#include "config.h"
#include "compat.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define NUM_LOGS 20
#define LOG_DIR ".tup/log"
#define LOG_NAME ".tup/log/debug.log.0"

static int enabled = 0;
static FILE *logfile = NULL;

static int get_log(const char *name, char *buf, int size, int num)
{
	if(snprintf(buf, size, "%s.%i", name, num) >= size) {
		fprintf(stderr, "tup internal error: log file buffer is sized incorrectly.\n");
		return -1;
	}
	return 0;
}

static int rotate(const char *name)
{
	int fd;
	int i;
	char buf[PATH_MAX];
	char buf2[PATH_MAX];

	fd = open(LOG_DIR, O_RDONLY);
	if(fd < 0) {
		/* No log directory, so there's nothing to rotate. */
		return 0;
	}

	if(get_log(name, buf, PATH_MAX, NUM_LOGS-1) < 0)
		return -1;
	if(unlinkat(fd, buf, 0) < 0) {
		if(errno != ENOENT) {
			perror(buf);
			fprintf(stderr, "tup error: Unable to unlink log file: %s\n", buf);
			return -1;
		}
	}
	for(i=NUM_LOGS-1; i>0; i--) {
		get_log(name, buf, PATH_MAX, i);
		get_log(name, buf2, PATH_MAX, i-1);
		if(renameat(fd, buf2, fd, buf) < 0) {
			if(errno != ENOENT) {
				perror("renameat");
				fprintf(stderr, "tup error: Unable to rotate logs.\n");
				return -1;
			}
		}
	}
	return 0;
}

void logging_enable(int argc, char **argv)
{
	int x;
	char timedata[64];
	struct tm tm;
	time_t t;

	time(&t);
#ifdef _WIN32
	localtime_s(&tm, &t);
#else
	localtime_r(&t, &tm);
#endif
	if(strftime(timedata, sizeof(timedata), "%c", &tm) <= 0) {
		timedata[0] = 0;
	}

	if(rotate("debug.log") < 0) {
		return;
	}
	if(rotate("create.dot") < 0) {
		return;
	}
	if(rotate("update.dot") < 0) {
		return;
	}
	if(mkdirat(tup_top_fd(), LOG_DIR, 0777) < 0) {
		if(errno != EEXIST) {
			perror(LOG_DIR);
			fprintf(stderr, "tup error: Unable to create debug log directory.\n");
			enabled = 0;
			return;
		}
	}
	logfile = fopen(LOG_NAME, "w+");
	if(!logfile) {
		perror(LOG_NAME);
		fprintf(stderr, "tup error: Unable to create debug log.\n");
		enabled = 0;
		return;
	}
	enabled = 1;
	log_debug("Tup update at %s:", timedata);
	for(x=0; x<argc; x++) {
		log_debug(" '%s'", argv[x]);
	}
	log_debug("\n");
}

void log_debug(const char *format, ...)
{
	va_list ap;

	if(!enabled)
		return;
	va_start(ap, format);
	vfprintf(logfile, format, ap);
	va_end(ap);
}

void log_debug_tent(const char *identifier, struct tup_entry *tent, const char *format, ...)
{
	va_list ap;
	if(!enabled)
		return;
	fprintf(logfile, "%s[%lli]: ", identifier, tent->tnode.tupid);
	print_tup_entry(logfile, tent);

	va_start(ap, format);
	vfprintf(logfile, format, ap);
	va_end(ap);
}

void log_graph(struct graph *g, const char *name)
{
	if(enabled) {
		char fullname[PATH_MAX];
		if(snprintf(fullname, sizeof(fullname), ".tup/log/%s.dot.0", name) >= (signed)sizeof(fullname)) {
			fprintf(stderr, "tup internal error: log graph name is sized incorrectly.\n");
			return;
		}
		save_graph(NULL, g, fullname);
	}
}
