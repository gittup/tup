/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2011-2014  Mike Shal <marfey@gmail.com>
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

#ifndef tup_pel_group_h
#define tup_pel_group_h

#include "tupid.h"
#include "bsd/queue.h"

struct path_element {
	TAILQ_ENTRY(path_element) list;
	const char *path; /* Not nul-terminated */
	int len;
};
TAILQ_HEAD(path_element_head, path_element);

#define PG_HIDDEN 1
#define PG_OUTSIDE_TUP 2
#define PG_ROOT 4
#define PG_GROUP 8
struct pel_group {
	struct path_element_head path_list;
	int pg_flags;
	int num_elements;
};

void init_pel_group(struct pel_group *pg);
int get_path_tupid(struct pel_group *pg, tupid_t *tupid);
int get_path_elements(const char *dir, struct pel_group *pg);
int append_path_elements(struct pel_group *pg, tupid_t dt);
int pg_eq(const struct pel_group *pga, const struct pel_group *pgb);
void del_pel(struct path_element *pel, struct pel_group *pg);
void del_pel_group(struct pel_group *pg);
void print_pel_group(struct pel_group *pg);

#endif
