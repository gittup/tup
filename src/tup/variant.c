/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2012-2014  Mike Shal <marfey@gmail.com>
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
#include "db.h"
#include "container.h"
#include <stdio.h>
#include <stdlib.h>

static struct variant_head variant_list = LIST_HEAD_INITIALIZER(&variant_list);
static struct variant_head disabled_list = LIST_HEAD_INITIALIZER(&disabled_list);
static struct tupid_entries variant_root = RB_INITIALIZER(&variant_root);
static struct tupid_entries variant_dt_root = RB_INITIALIZER(&variant_dt_root);

static int load_cb(void *arg, struct tup_entry *tent)
{
	if(arg) {}

	if(variant_add(tent, 1, NULL) < 0)
		return -1;
	return 0;
}

int variant_load(void)
{
	if(tup_db_select_node_by_flags(load_cb, NULL, TUP_FLAGS_VARIANT) < 0)
		return -1;
	return 0;
}

static int initialize_variant(struct variant *variant)
{
	vardb_init(&variant->vdb);
	LIST_INSERT_HEAD(&variant_list, variant, list);
	if(tupid_tree_insert(&variant_dt_root, &variant->dtnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert variant for node %lli into the tree in variant_add()\n", variant->dtnode.tupid);
		return -1;
	}
	if(tupid_tree_insert(&variant_root, &variant->tnode) < 0) {
		fprintf(stderr, "tup error: Unable to insert variant for node %lli into the tree in variant_add()\n", variant->tnode.tupid);
		return -1;
	}
	return 0;
}

int variant_add(struct tup_entry *tent, int enabled, struct variant **dest)
{
	struct variant *variant;

	variant = malloc(sizeof *variant);
	if(!variant) {
		perror("malloc");
		return -1;
	}
	variant->tent = tent;

	/* We search based on the directory, not tup.config */
	variant->dtnode.tupid = tent->dt;

	/* But when we remove from variant_list, we search based on our actual
	 * tupid.
	 */
	variant->tnode.tupid = tent->tnode.tupid;

	variant->enabled = enabled;
	if(snprint_tup_entry(variant->variant_dir, sizeof(variant->variant_dir), tent->parent) >= (signed)sizeof(variant->variant_dir)) {
		fprintf(stderr, "tup internal error: variant_dir is sized incorrectly.\n");
		return -1;
	}

	if(tent->dt == DOT_DT) {
		variant->root_variant = 1;
		variant->vardict_len = snprintf(variant->vardict_file, sizeof(variant->vardict_file), ".tup/vardict") + 1;
	} else {
		variant->root_variant = 0;
		variant->vardict_len = snprintf(variant->vardict_file, sizeof(variant->vardict_file), ".tup/vardict-%s", variant->variant_dir+1) + 1;
	}
	if(variant->vardict_len >= (signed)sizeof(variant->vardict_file)) {
		fprintf(stderr, "tup error: variant vardict_file is sized incorrectly.\n");
		return -1;
	}

	if(initialize_variant(variant) < 0)
		return -1;

	if(dest)
		*dest = variant;

	return 0;
}

int variant_rm(struct variant *variant)
{
	/* Just disable the variant and remove it from the structures, since
	 * some tup_entrys may already point to us.
	 */
	variant->enabled = 0;
	tupid_tree_rm(&variant_dt_root, &variant->dtnode);
	tupid_tree_rm(&variant_root, &variant->tnode);
	LIST_REMOVE(variant, list);
	vardb_close(&variant->vdb);
	LIST_INSERT_HEAD(&disabled_list, variant, list);
	return 0;
}

int variant_enable(struct variant *variant)
{
	variant->enabled = 1;
	if(initialize_variant(variant) < 0)
		return -1;
	return 0;
}

struct variant *variant_search(tupid_t dt)
{
	struct tupid_tree *tt;

	tt = tupid_tree_search(&variant_dt_root, dt);
	if(!tt)
		return NULL;
	return container_of(tt, struct variant, dtnode);
}

struct variant_head *get_variant_list(void)
{
	return &variant_list;
}

int variant_list_empty(void)
{
	return LIST_EMPTY(&variant_list);
}

int variant_get_srctent(struct variant *variant, tupid_t tupid, struct tup_entry **srctent)
{
	struct tup_entry *tent;

	*srctent = NULL;
	if(variant->root_variant)
		return 0;

	if(tup_entry_add(tupid, &tent) < 0)
		return -1;

	/* srcid can be -1 if the variant node is a ghost */
	if(tent->srcid != -1)
		if(tup_entry_add(tent->srcid, srctent) < 0)
			return -1;
	return 0;
}

void variants_free(void)
{
	struct variant *variant;
	while(!LIST_EMPTY(&variant_list)) {
		variant = LIST_FIRST(&variant_list);
		tupid_tree_rm(&variant_dt_root, &variant->dtnode);
		tupid_tree_rm(&variant_root, &variant->tnode);
		LIST_REMOVE(variant, list);
		vardb_close(&variant->vdb);
		free(variant);
	}
	while(!LIST_EMPTY(&disabled_list)) {
		variant = LIST_FIRST(&disabled_list);
		LIST_REMOVE(variant, list);
		free(variant);
	}
}
