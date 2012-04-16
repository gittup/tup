/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2011  Mike Shal <marfey@gmail.com>
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

#ifndef tup_vardb_h
#define tup_vardb_h

#include "string_tree.h"
#include "bsd/queue.h"

struct tup_entry;

struct vardb {
	struct string_entries root;
	int count;
};

struct var_tent_list_entry {
	TAILQ_ENTRY(var_tent_list_entry) list;
	struct tup_entry *tent;
};
TAILQ_HEAD(var_tent_list_head, var_tent_list_entry);

struct var_entry {
	struct string_tree var;
	char *value;
	int vallen;
	struct tup_entry *tent;   /* only used in db.c */
	struct var_tent_list_head tlist;   /* used for node variables */
};

int vardb_init(struct vardb *v);
int vardb_close(struct vardb *v);
int vardb_set(struct vardb *v, const char *var, const char *value,
	      struct tup_entry *tent);
struct var_entry *vardb_set2(struct vardb *v, const char *var, int varlen,
			     const char *value, struct tup_entry *tent);
int vardb_append(struct vardb *v, const char *var, const char *value,
                 struct tup_entry *tent);
int vardb_len(struct vardb *v, const char *var, int varlen);
int vardb_copy(struct vardb *v, const char *var, int varlen, char **dest);
struct var_entry *vardb_get(struct vardb *v, const char *var, int varlen);
int vardb_compare(struct vardb *vdba, struct vardb *vdbb,
		  int (*extra_a)(struct var_entry *ve),
		  int (*extra_b)(struct var_entry *ve),
		  int (*same)(struct var_entry *vea, struct var_entry *veb));
void vardb_dump(struct vardb *v);

#endif
