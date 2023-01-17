/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012-2023  Mike Shal <marfey@gmail.com>
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

#ifndef tup_variant_h
#define tup_variant_h

#include "bsd/queue.h"
#include "compat.h"
#include "tupid_tree.h"
#include "vardb.h"

#define VARIANT_SRCDIR_REMOVED -2

struct variant {
	struct tupid_tree dtnode;
	struct tupid_tree tnode;
	LIST_ENTRY(variant) list;
	struct tup_entry *tent;
	struct vardb vdb;
	int enabled;
	int root_variant;
	int vardict_len;
	char variant_dir[PATH_MAX];
	char vardict_file[PATH_MAX];
};
LIST_HEAD(variant_head, variant);

int variant_load(void);
int variant_add(struct tup_entry *tent, int enabled, struct variant **dest);
int variant_rm(struct variant *variant);
int variant_enable(struct variant *variant);
struct variant *variant_search(tupid_t dt);
struct variant_head *get_variant_list(void);
int variant_list_empty(void);
int variant_get_srctent(struct variant *variant, struct tup_entry *tent, struct tup_entry **srctent);
struct tup_entry *variant_tent_to_srctent(struct tup_entry *tent);
void variants_free(void);

#endif
