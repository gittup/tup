/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2015  Mike Shal <marfey@gmail.com>
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

#include "tupid.h"
#include "entry.h"
#include "string_tree.h"

struct tup_entry;

struct vardb {
	struct string_entries root;
	int count;
};

struct var_entry {
	struct string_tree var;
	char *value;
	int vallen;
	struct tup_entry *tent;   /* only used in db.c */
};

int vardb_init(struct vardb *v);
int vardb_close(struct vardb *v);
int vardb_set(struct vardb *v, const char *var, const char *value,
              struct tup_entry *tent);
struct var_entry *vardb_set2(struct vardb *v, const char *var, int varlen,
                             const char *value, struct tup_entry *tent);
int vardb_append(struct vardb *v, const char *var, const char *value);
int vardb_len(struct vardb *v, const char *var, int varlen);
int vardb_copy(struct vardb *v, const char *var, int varlen, char **dest);
struct var_entry *vardb_get(struct vardb *v, const char *var, int varlen);
int vardb_compare(struct vardb *vdba, struct vardb *vdbb,
		  int (*extra_a)(struct var_entry *ve, tupid_t vardt),
		  int (*extra_b)(struct var_entry *ve, tupid_t vardt),
		  int (*same)(struct var_entry *vea, struct var_entry *veb),
		  tupid_t vardt);
void vardb_dump(struct vardb *v);

/* Node variables */

struct node_vardb {
   struct string_entries root;
   int count;
};

struct node_var_entry {
   struct string_tree var;
   struct tent_list_head nodes;
};

int nodedb_init(struct node_vardb *v);
int nodedb_close(struct node_vardb *v);
int nodedb_set(struct node_vardb *v, const char *var,
               struct tup_entry *tent);
int nodedb_append(struct node_vardb *v, const char *var,
                  struct tup_entry *tent);
int nodedb_len(struct node_vardb *v, const char *var, int varlen,
               tupid_t relative_to);
int nodedb_copy(struct node_vardb *v, const char *var, int varlen,
                char **dest, tupid_t relative_to);
struct node_var_entry *nodedb_get(struct node_vardb *v,
                                  const char *var, int varlen);

#endif
