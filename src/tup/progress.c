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
#include "db.h"
#include "entry.h"
#include <stdio.h>
#include <unistd.h>

static int cur_phase = -1;
static int sum;
static int total;
static int is_active = 0;
static int stdout_isatty;
static int bar_width;
static const char *bar_fmt, *bar_fmt_percent;

void progress_init(void)
{
	stdout_isatty = isatty(STDOUT_FILENO);
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

static void clear_active(FILE *f)
{
	if(is_active) {
		printf("\r                             \r");
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

	if (total > 9999) {
		/* If it's a good enough limit for Final Fantasy VII, it's good
		 * enough for me.
		 */
		bar_width = 7;
		bar_fmt_percent = " %3i%%  ";
		bar_fmt = NULL; // not used
	} else if (total > 999) {
		bar_width = 11;
		bar_fmt_percent = "   %3i%%    ";
		bar_fmt = " %4i/%-4i ";
	} else if (total > 99) {
		bar_width = 9;
		bar_fmt_percent = "  %3i%%   ";
		bar_fmt = " %3i/%-3i ";
	} else if (total > 9) {
		bar_width = 7;
		bar_fmt_percent = " %3i%%  ";
		bar_fmt = " %2i/%-2i ";
	} else {
		bar_width = 5;
		bar_fmt_percent = "%3i%% ";
		bar_fmt = " %1i/%-1i ";
	}
}

static void show_bar(FILE *f, int node_type, int show_percent)
{
	if(total) {
		int fill;
		char buf[12];

		clear_active(f);

		if(total > 9999 || show_percent) {
			snprintf(buf, sizeof(buf), bar_fmt_percent, sum*100/total);
		} else {
			snprintf(buf, sizeof(buf), bar_fmt, sum, total);
		}
		/* TUP_NODE_ROOT means an error - fill the whole bar so it's
		 * obvious.
		 */
		if(node_type == TUP_NODE_ROOT || sum == total)
			fill = bar_width;
		else
			fill = (bar_width + 1) * sum / total;

		color_set(f);
		fprintf(f, "[%s%s%.*s%s%.*s] ", color_type(node_type), color_append_reverse(), fill, buf, color_end(), bar_width-fill, buf+fill);
	}
}

void show_progress(struct tup_entry *tent, int is_error)
{
	FILE *f;

	sum++;
	if(is_error) {
		f = stderr;
		show_bar(f, TUP_NODE_ROOT, 0);
	} else {
		f = stdout;
		show_bar(f, tent->type, 0);
	}
	print_tup_entry(f, tent);
	fprintf(f, "\n");
}

void show_active(int active)
{
	if(total && stdout_isatty) {
		/* First time through we should 0/N for the progress bar, then
		 * after that we just show the percentage complete, since the
		 * previous line will have a 1/N line for the last completed
		 * job.
		 */
		show_bar(stdout, TUP_NODE_CMD, sum != 0);
		printf("Active: %i", active);
		fflush(stdout);
		is_active = 1;
	}
}
