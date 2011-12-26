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

#include "progress.h"
#include "colors.h"
#include "db_types.h"
#include "entry.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

static int cur_phase = -1;
static int sum;
static int total;
static int is_active = 0;
static int stdout_isatty;
static int console_width;
static int color_len;
static int got_error = 0;

static int get_console_width(void)
{
#ifdef TIOCGWINSZ
	struct winsize wsz;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz) < 0)
		return 0;

	return wsz.ws_col;
#elif defined(WINDOWS)
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if(!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
		return 0;
	return csbi.dwSize.X;
#else
	return 0;
#endif
}

void progress_init(void)
{
	stdout_isatty = isatty(STDOUT_FILENO);
	console_width = get_console_width();
	color_len = strlen(color_type(TUP_NODE_CMD)) +
		strlen(color_append_reverse()) +
		strlen(color_end());
}

void tup_show_message(const char *s)
{
	const char *tup = " tup ";
	color_set(stdout);
	/* If we get to the end, show a green bar instead of grey. */
	if(cur_phase == 5)
		printf("[%s%s%s] %s", color_final(), tup, color_end(), s);
	else
		printf("[%s%.*s%s%.*s] %s", color_reverse(), cur_phase, tup, color_end(), 5-cur_phase, tup+cur_phase, s);
}

void clear_active(FILE *f)
{
	if(is_active) {
		char spaces[console_width];
		memset(spaces, ' ', console_width);
		printf("\r%.*s\r", console_width, spaces);
		is_active = 0;
		if(f == stderr)
			fflush(stdout);
	}
}

void tup_main_progress(const char *s)
{
	clear_active(stdout);
	cur_phase++;
	tup_show_message(s);
}

void start_progress(int new_total)
{
	sum = 0;
	total = new_total;
}

void show_progress(struct tup_entry *tent, int is_error)
{
	FILE *f;
	int node_type = tent->type;

	sum++;
	if(is_error) {
		got_error = 1;
		f = stderr;
		tent->type = TUP_NODE_ROOT;
	} else {
		f = stdout;
	}
	clear_active(f);
	color_set(f);
	if(is_error) {
		fprintf(stderr, "* ");
	} else {
		printf(" ");
	}
	fprintf(f, "%i) ", sum);
	print_tup_entry(f, tent);
	fprintf(f, "\n");
	tent->type = node_type;
}

void show_active(int active, int type)
{
	if(total && stdout_isatty && console_width >= 10) {
		/* -3 for the [] and leading space, and -6 for the " 100% " at
		 * the end.
		 */
		const int max = console_width - 9;
		int fill;
		char buf[console_width + color_len];
		char activebuf[32];
		int activebuflen = 0;
		char remainingbuf[32];
		int remainingbuflen;
		int offset;

		if(got_error)
			type = TUP_NODE_ROOT;

		clear_active(stdout);

		if(active != -1) {
			activebuflen = snprintf(activebuf, sizeof(activebuf), "Active=%i", active);
		}
		remainingbuflen = snprintf(remainingbuf, sizeof(remainingbuf), "Remaining=%i", total-sum);
		if(max > activebuflen + remainingbuflen) {
			offset = (max - (activebuflen + remainingbuflen)) / 2;

			memset(buf, ' ', offset);
			offset += snprintf(buf + offset, sizeof(buf) - offset, "%.*s %.*s", activebuflen, activebuf, remainingbuflen, remainingbuf);
			memset(buf + offset, ' ', sizeof(buf) - offset);
		} else {
			memset(buf, ' ', sizeof(buf));
		}

		fill = max * sum / total;
		color_set(stdout);
		printf(" [%s%s%.*s%s%.*s] %3i%%", color_type(type), color_append_reverse(), fill, buf, color_end(), max-fill, buf+fill, sum*100/total);
		if(sum == total)
			printf("\n");
		else
			is_active = 1;
		fflush(stdout);
	}
}
