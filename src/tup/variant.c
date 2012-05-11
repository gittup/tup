/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012  Mike Shal <marfey@gmail.com>
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

#include "variant.h"
#include "entry.h"
#include "db_types.h"
#include "container.h"
#include <stdio.h>
#include <stdlib.h>

static struct tupid_entries variant_root = RB_INITIALIZER(&variant_root);

int variant_add(struct variant_head *head, struct tup_entry *tent, int enabled)
{
	struct variant *variant;

	variant = malloc(sizeof *variant);
	if(!variant) {
		perror("malloc");
		return -1;
	}
	variant->tent = tent;

	/* We search based on the directory, not tup.config */
	variant->tnode.tupid = tent->dt;
	if(tent->dt == DOT_DT)
		variant->root_variant = 1;
	else
		variant->root_variant = 0;

	vardb_init(&variant->vdb);
	variant->enabled = enabled;
	if(snprint_tup_entry(variant->variant_dir, sizeof(variant->variant_dir), tent->parent) >= (signed)sizeof(variant->variant_dir)) {
		fprintf(stderr, "tup internal error: variant_dir is sized incorrectly.\n");
		return -1;
	}
	LIST_INSERT_HEAD(head, variant, list);
	if(tupid_tree_insert(&variant_root, &variant->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert variant for node %lli into the tree in variant_add()\n", variant->tnode.tupid);
		return -1;
	}

	return 0;
}

int variant_rm(tupid_t dt)
{
	struct variant *variant;

	variant = variant_search(dt);
	if(!variant) {
		fprintf(stderr, "tup internal error: Unable to find variant for node %lli in variant_rm()\n", dt);
		return -1;
	}
	tupid_tree_rm(&variant_root, &variant->tnode);
	LIST_REMOVE(variant, list);
	free(variant);
	return 0;
}

struct variant *variant_search(tupid_t dt)
{
	struct tupid_tree *tt;

	tt = tupid_tree_search(&variant_root, dt);
	if(!tt)
		return NULL;
	return container_of(tt, struct variant, tnode);
}
