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

#ifndef tup_progress_h
#define tup_progress_h

#include "db_types.h"
#include <stdio.h>

struct tup_entry;
struct timespan;

void progress_init(void);
void tup_show_message(const char *s);
void tup_main_progress(const char *s);
void start_progress(int new_total, int new_total_time, int new_max_jobs);
void skip_result(struct tup_entry *tent);
void show_result(struct tup_entry *tent, int is_error, struct timespan *ts, const char *extra_text, int always_display);
void show_progress(int active, enum TUP_NODE_TYPE type);
void clear_active(FILE *f);
void clear_progress(void);
void progress_quiet(void);

#endif
