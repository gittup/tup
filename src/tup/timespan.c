/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2017  Mike Shal <marfey@gmail.com>
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

#include <stdio.h>
#include "timespan.h"

void timespan_start(struct timespan *ts)
{
	gettimeofday(&ts->start, NULL);
}

void timespan_end(struct timespan *ts)
{
	gettimeofday(&ts->end, NULL);
}

time_t timespan_milliseconds(struct timespan *ts)
{
	/* The +500 is to round to the nearest ms */
	return (ts->end.tv_sec - ts->start.tv_sec) * 1000 +
		(ts->end.tv_usec - ts->start.tv_usec + 500) / 1000;
}

float timespan_seconds(struct timespan *ts)
{
	return (float)(ts->end.tv_sec - ts->start.tv_sec) +
		(float)(ts->end.tv_usec - ts->start.tv_usec)/1e6;
}

void timespan_add_delta(struct timespan *ts, const struct timespan *delta)
{
	ts->start.tv_sec += delta->end.tv_sec - delta->start.tv_sec;
	ts->start.tv_usec += delta->end.tv_usec - delta->start.tv_usec;
	if(ts->start.tv_usec >= 1000000) {
		ts->start.tv_usec -= 1000000;
		ts->start.tv_sec++;
	} else if(ts->start.tv_usec < 0) {
		ts->start.tv_usec += 1000000;
		ts->start.tv_sec--;
	}
}
