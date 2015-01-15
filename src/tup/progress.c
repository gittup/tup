/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2015  Mike Shal <marfey@gmail.com>
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
#include "option.h"
#include "entry.h"
#include "timespan.h"
#include "array_size.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

static int cur_phase = -1;
static int sum;
static int total;
static int job_time;
static int total_time;
static int max_jobs;
static int is_active = 0;
static int color_len;
static int got_error = 0;
static int display_progress;
static int quiet;
static int display_job_numbers;
static int display_job_time;
static struct timespan gts;
static struct timespan main_ts;

static int get_time_remaining(char *dest, int len, int part, int whole, int approx);

/* Each of these corresponds to one unit of info that can be displayed inside
 * the progress bar (eg: ETA, Remaining, Active). Maxlen remains constant for
 * the duration of the progress bar. We expect the size of the text to decrease
 * during its lifetime as the numbers go down.  This prevents the fields from
 * moving within the progress bar, or suddenly adding a field as the numbers
 * change.
 */
struct {
	char text[32];
	int len;
	int maxlen;
} infos[3];

void progress_init(void)
{
	color_len = strlen(color_type(TUP_NODE_CMD)) +
		strlen(color_append_reverse()) +
		strlen(color_end());
	display_progress = tup_option_get_int("display.progress");
	display_job_numbers = tup_option_get_int("display.job_numbers");
	display_job_time = tup_option_get_int("display.job_time");
	quiet = tup_option_get_int("display.quiet");
	timespan_start(&main_ts);
}

void progress_quiet(void)
{
	quiet = 1;
}

void tup_show_message(const char *s)
{
	const char *tup = " tup ";
	if(quiet)
		return;
	clear_progress();
	color_set(stdout);
	/* If we get to the end, show a green bar instead of grey. */
	if(cur_phase == 5)
		printf("[%s%s%s] ", color_final(), tup, color_end());
	else
		printf("[%s%.*s%s%.*s] ", color_reverse(), cur_phase, tup, color_end(), 5-cur_phase, tup+cur_phase);
	timespan_end(&main_ts);
	if(display_job_time)
		printf("[%.3fs] ", timespan_seconds(&main_ts));
	printf("%s", s);
}

void clear_active(FILE *f)
{
	if(is_active) {
		/* Subtract 1 so we don't scroll to the next line if we print
		 * exactly the correct amount of characters for a row. At least
		 * Windows does this.
		 */
		int console_width = tup_option_get_int("display.width") - 1;
		char spaces[console_width];
		memset(spaces, ' ', console_width);
		printf("\r%.*s\r", console_width, spaces);
		is_active = 0;
		if(f == stderr)
			fflush(stdout);
	}
}

void clear_progress(void)
{
	if(is_active) {
		printf("\n");
	}
}

void tup_main_progress(const char *s)
{
	clear_active(stdout);
	cur_phase++;
	tup_show_message(s);
}

void start_progress(int new_total, int new_total_time, int new_max_jobs)
{
	int i;

	sum = 0;
	total = new_total;
	job_time = 0;
	total_time = new_total_time;
	max_jobs = new_max_jobs;

	i = 0;

	/* Use the time if we have that available, otherwise fallback to the
	 * number of jobs.
	 */
	if(total_time != -1) {
		infos[i].maxlen = get_time_remaining(infos[i].text, sizeof(infos[i].text), 0, total_time, 0);
	} else {
		infos[i].maxlen = get_time_remaining(infos[i].text, sizeof(infos[i].text), 0, total, 1);
	}
	i++;

	infos[i].maxlen = snprintf(infos[i].text, sizeof(infos[i].text), "Remaining=%i", total);
	i++;

	infos[i].maxlen = snprintf(infos[i].text, sizeof(infos[i].text), "Active=%i", max_jobs);
	i++;

	timespan_start(&gts);
}

void skip_result(struct tup_entry *tent)
{
	sum++;
	if(tent) {
		total_time -= tent->mtime;
	}
}

static int percent_complete(void)
{
	if(!total)
		return 0;
	/* Use the job times if we have them available since it will report
	 * a more accurate percentage complete than just the job count.
	 */
	if(total_time > 0) {
		return (job_time*100)/total_time;
	}

	/* Default to job count if we can't use previous execution times. */
	return (sum*100)/total;
}

void show_result(struct tup_entry *tent, int is_error, struct timespan *ts, const char *extra_text, int always_display)
{
	FILE *f;
	float tdiff = 0.0;

	job_time += tent->mtime;

	if(ts) {
		tdiff = timespan_seconds(ts);
	}

	sum++;
	if(sum > total) {
		fprintf(stderr, "tup internal error: progress bar is sized incorrectly.\n");
	}

	if(quiet && !always_display)
		return;

	if(is_error) {
		got_error = 1;
		f = stderr;
	} else {
		f = stdout;
	}
	clear_active(f);
	color_set(f);
	if(is_error) {
		fprintf(stderr, "* %s", color_error_mode());
	} else {
		printf(" ");
	}
	/* If we aren't going to show a progress bar, then %-complete here is
	 * helpful.
	 */
	if(total && !display_progress) fprintf(f, "%3i%% ", percent_complete());
	if(display_job_numbers) fprintf(f, "%i) ", sum);
	if(display_job_time && ts) {
		fprintf(f, "[%.3fs] ", tdiff);
	}

	if(extra_text)
		fprintf(f, "%s: ", extra_text);

	print_tup_entry(f, tent);
	fprintf(f, "\n");
	color_error_mode_clear();
}

static int get_time_remaining(char *dest, int len, int part, int whole, int approx)
{
	const char *eq = "=";

	if(approx)
		eq = "~=";
	if(part != 0) {
		time_t ms;
		time_t total_runtime;
		time_t time_left;

		timespan_end(&gts);
		ms = timespan_milliseconds(&gts);

		/* This can easily overflow, so do the computation in float */
		total_runtime = (time_t)((float)ms * (float)whole / (float)part);
		time_left = total_runtime - ms;

		/* Try to find the best units. Note that we use +1 because the
		 * division always rounds down, so eg 1.9s would become 1s.
		 * With the +1 we always round up, so 1.0m-2.0m shows as '2m'. Once
		 * we go down to 1m, we switch over to displaying in seconds (eg:
		 * 60s).
		 *
		 * Also note that the length of each text field has a max size of
		 * "ETA=???". This keeps the len field in the info structure
		 * to be monotonically decreasing over time.
		 */
		if(time_left < 1000) {
			return snprintf(dest, len, "ETA%s<1s", eq);
		} else if(time_left < 60000) {
			return snprintf(dest, len, "ETA%s%lis", eq, (long int)time_left/1000 + 1);
		} else if(time_left < 3600000) {
			return snprintf(dest, len, "ETA%s%lim", eq, (long int)time_left/60000 + 1);
		} else if(time_left < 356400000) {
			return snprintf(dest, len, "ETA%s%lih", eq, (long int)time_left/3600000 + 1);
		}
	}
	return snprintf(dest, len, "ETA%s???", eq);
}

void show_progress(int active, enum TUP_NODE_TYPE type)
{
	int console_width = tup_option_get_int("display.width");
	if(total && display_progress && console_width >= 10) {
		/* -3 for the [] and leading space, and -6 for the " 100% " at
		 * the end.
		 */
		int max = console_width - 9;
		int fill;
		char buf[console_width + color_len + 1];
		int num_infos = 0;
		int i = 0;
		int x;
		int tmpmax;
		int offset;

		if(max > total)
			max = total;
		if(got_error)
			type = TUP_NODE_ROOT;

		clear_active(stdout);

		fill = max * sum / total;

		if(color_len) {
			memset(buf, ' ', sizeof(buf));
		} else {
			memset(buf, '.', fill);
			memset(buf+fill, ' ', sizeof(buf) - fill);
		}

		for(x=0; x<ARRAY_SIZE(infos); x++) {
			infos[x].len = 0;
		}

		if(total_time != -1) {
			infos[i].len = get_time_remaining(infos[i].text, sizeof(infos[i].text), job_time, total_time, 0);
		} else {
			infos[i].len = get_time_remaining(infos[i].text, sizeof(infos[i].text), sum, total, 1);
		}
		i++;

		infos[i].len = snprintf(infos[i].text, sizeof(infos[i].text), "Remaining=%i", total-sum);
		i++;

		if(active != -1) {
			infos[i].len = snprintf(infos[i].text, sizeof(infos[i].text), "Active=%i", active);
		} else {
			/* Override maxlen to disable "Active..." */
			infos[i].maxlen = 0;
		}
		i++;

		tmpmax = max;
		for(x=0; x<ARRAY_SIZE(infos); x++) {
			int spacing = 0;
			if(x)
				spacing = 1;
			if(tmpmax >= infos[x].maxlen + spacing) {
				tmpmax -= infos[x].maxlen + spacing;
				num_infos++;
			} else {
				break;
			}
		}
		offset = tmpmax / 2;
		for(x=0; x<num_infos; x++) {
			if(x)
				offset++;
			memcpy(buf + offset, infos[x].text, infos[x].len);
			offset += infos[x].maxlen;
		}

		color_set(stdout);
		printf(" [%s%s%.*s%s%.*s] %3i%%", color_type(type), color_append_reverse(), fill, buf, color_end(), max-fill, buf+fill, percent_complete());
		is_active = 1;
		fflush(stdout);
	}
}
