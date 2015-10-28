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

#include "vardb.h"
#include "container.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "entry.h"

int vardb_init(struct vardb *v)
{
	RB_INIT(&v->root);
	v->count = 0;
	return 0;
}

int vardb_close(struct vardb *v)
{
	struct string_tree *st;

	while((st = RB_ROOT(&v->root)) != NULL) {
		struct var_entry *ve = container_of(st, struct var_entry, var);
		string_tree_rm(&v->root, st);
		free(st->s);
		free(ve->value);
		free(ve);
	}
	return 0;
}

int vardb_set(struct vardb *v, const char *var, const char *value,
	      struct tup_entry *tent)
{
	if(vardb_set2(v, var, strlen(var), value, tent) == NULL)
		return -1;
	return 0;
}

struct var_entry *vardb_set2(struct vardb *v, const char *var, int varlen,
			     const char *value, struct tup_entry *tent)
{
	struct string_tree *st;
	struct var_entry *ve;
	int vallen = 0;

	if(value)
		vallen = strlen(value);

	st = string_tree_search(&v->root, var, varlen);
	if(st) {
		ve = container_of(st, struct var_entry, var);

		free(ve->value);
		ve->vallen = vallen;
		if(value) {
			ve->value = malloc(ve->vallen + 1);
			if(!ve->value) {
				perror("malloc");
				return NULL;
			}
			strcpy(ve->value, value);
		} else {
			ve->value = NULL;
		}
		ve->tent = tent;
	} else {
		ve = malloc(sizeof *ve);
		if(!ve) {
			perror("malloc");
			return NULL;
		}

		if(varlen == -1)
			varlen = strlen(var);
		ve->var.len = varlen;
		ve->var.s = malloc(ve->var.len + 1);
		if(!ve->var.s) {
			perror("malloc");
			free(ve);
			return NULL;
		}
		memcpy(ve->var.s, var, varlen);
		ve->var.s[varlen] = 0;
		ve->vallen = vallen;
		if(value) {
			ve->value = malloc(ve->vallen + 1);
			if(!ve->value) {
				perror("malloc");
				free(ve->var.s);
				free(ve);
				return NULL;
			}
			strcpy(ve->value, value);
		} else {
			ve->value = NULL;
		}
		ve->tent = tent;

		if(string_tree_insert(&v->root, &ve->var) < 0) {
			fprintf(stderr, "vardb_set: Error inserting into tree\n");
			free(ve->value);
			free(ve->var.s);
			free(ve);
			return NULL;
		}

		v->count++;
	}
	return ve;
}

int vardb_append(struct vardb *v, const char *var, const char *value)
{
	struct string_tree *st;

	st = string_tree_search(&v->root, var, strlen(var));
	if(st) {
		int vallen;
		char *new;
		struct var_entry *ve = container_of(st, struct var_entry, var);

		vallen = strlen(value);
		new = malloc(ve->vallen + vallen + 2);
		if(!new) {
			perror("malloc");
			return -1;
		}
		memcpy(new, ve->value, ve->vallen);
		new[ve->vallen] = ' ';
		memcpy(new+ve->vallen+1, value, vallen);
		new[ve->vallen+vallen+1] = 0;
		free(ve->value);
		ve->value = new;
		ve->vallen += vallen + 1;
		return 0;
	} else {
		return vardb_set(v, var, value, NULL);
	}
}

int vardb_copy(struct vardb *v, const char *var, int varlen, struct estring *e)
{
	struct string_tree *st;

	st = string_tree_search(&v->root, var, varlen);
	if(st) {
		struct var_entry *ve = container_of(st, struct var_entry, var);
		if(estring_append(e, ve->value, ve->vallen) < 0)
			return -1;
		return 0;
	}
	/* Variable not found: string is "" */
	return 0;
}

struct var_entry *vardb_get(struct vardb *v, const char *var, int varlen)
{
	struct string_tree *st;

	st = string_tree_search(&v->root, var, varlen);
	if(st) {
		struct var_entry *ve = container_of(st, struct var_entry, var);
		return ve;
	}
	return NULL;
}

int vardb_compare(struct vardb *vdba, struct vardb *vdbb,
		  int (*extra_a)(struct var_entry *ve, tupid_t vardt),
		  int (*extra_b)(struct var_entry *ve, tupid_t vardt),
		  int (*same)(struct var_entry *vea, struct var_entry *veb),
		  tupid_t vardt)
{
	struct string_tree *sta;
	struct string_tree *stb;
	struct var_entry *vea;
	struct var_entry *veb;
	struct string_entries *a = &vdba->root;
	struct string_entries *b = &vdbb->root;

	sta = RB_MIN(string_entries, a);
	stb = RB_MIN(string_entries, b);

	while(sta || stb) {
		if(!sta) {
			veb = container_of(stb, struct var_entry, var);
			if(extra_b && extra_b(veb, vardt) < 0)
				return -1;
			stb = RB_NEXT(string_entries, b, stb);
		} else if(!stb) {
			vea = container_of(sta, struct var_entry, var);
			if(extra_a && extra_a(vea, vardt) < 0)
				return -1;
			sta = RB_NEXT(string_entries, a, sta);
		} else {
			int rc;
			vea = container_of(sta, struct var_entry, var);
			veb = container_of(stb, struct var_entry, var);
			rc = strcmp(sta->s, stb->s);
			if(rc == 0) {
				if(same && same(vea, veb) < 0)
					return -1;
				sta = RB_NEXT(string_entries, a, sta);
				stb = RB_NEXT(string_entries, b, stb);
			} else if(rc < 0) {
				if(extra_a && extra_a(vea, vardt) < 0)
					return -1;
				sta = RB_NEXT(string_entries, a, sta);
			} else {
				if(extra_b && extra_b(veb, vardt) < 0)
					return -1;
				stb = RB_NEXT(string_entries, b, stb);
			}
		}
	}
	return 0;
}

void vardb_dump(struct vardb *v)
{
	struct string_tree *st;

	printf(" ----------- VARDB -----------\n");
	RB_FOREACH(st, string_entries, &v->root) {
		struct var_entry *ve = container_of(st, struct var_entry, var);
		printf(" [%i] '%s' [33m=[0m [%i] '%s'\n", st->len, st->s, ve->vallen, ve->value);
	}
}

// todo - move this to entry.c ?
static int add_tent(struct tent_list_head *head, struct tup_entry *tent)
{
	struct tent_list *tlist = NULL;

	/* not an error, just add nothing to the list */
	if (!tent)
		return 0;

	tlist = malloc(sizeof *tlist);
	if(!tlist) {
		perror("malloc");
		return -1;
	}
	tlist->tent = tent;
	TAILQ_INSERT_TAIL(head, tlist, list);
	return 0;
}

int nodedb_init(struct node_vardb *v)
{
	RB_INIT(&v->root);
	v->count = 0;
	return 0;
}

int nodedb_close(struct node_vardb *v)
{
	struct string_tree *st;

	while((st = RB_ROOT(&v->root)) != NULL) {
		struct node_var_entry *ve = container_of(st, struct node_var_entry, var);
		string_tree_rm(&v->root, st);
		free(st->s);
		free_tent_list(&ve->nodes);
		free(ve);
	}
	return 0;
}

int nodedb_set(struct node_vardb *v, const char *var, struct tup_entry *tent)
{
	struct string_tree *st;
	struct node_var_entry *ve;

	// todo - use nodedb_get
	st = string_tree_search(&v->root, var, strlen(var));
	if(st) {
		ve = container_of(st, struct node_var_entry, var);
		free_tent_list(&ve->nodes);
		if (add_tent(&ve->nodes, tent) < 0)
			return -1;
	} else {
		ve = malloc(sizeof *ve);
		if(!ve) {
			perror("malloc");
			return -1;
		}

		ve->var.len = strlen(var);
		ve->var.s = malloc(ve->var.len + 1);
		if(!ve->var.s) {
			perror("malloc");
			free(ve);
			return -1;
		}
		memcpy(ve->var.s, var, ve->var.len);
		ve->var.s[ve->var.len] = 0;

		TAILQ_INIT(&ve->nodes);
		if (add_tent(&ve->nodes, tent) < 0) {
			free(ve->var.s);
			free(ve);
			return -1;
		}

		if(string_tree_insert(&v->root, &ve->var) < 0) {
			fprintf(stderr, "nodedb_set: Error inserting into tree\n");
			free(ve->var.s);
			free_tent_list(&ve->nodes);
			free(ve);
			return -1;
		}

		v->count++;
	}
	return 0;
}

int nodedb_append(struct node_vardb *v, const char *var, struct tup_entry *tent)
{
	struct string_tree *st;

	// todo - use nodedb_get
	st = string_tree_search(&v->root, var, strlen(var));
	if(st) {
		struct node_var_entry *ve = container_of(st, struct node_var_entry, var);
		return add_tent(&ve->nodes, tent);
	} else {
		return nodedb_set(v, var, tent);
	}
}

int nodedb_copy(struct node_vardb *v, const char *var, int varlen, struct estring *e,
                tupid_t relative_to)
{
	struct node_var_entry *ve = NULL;
	int clen = 0;
	int first = 0;
	int rc = -1;
	struct tent_list *tlist = NULL;

	ve = nodedb_get(v, var, varlen);
	if(!ve)
		return 0;	/* not found, string is "" */

	TAILQ_FOREACH(tlist, &ve->nodes, list) {
		if(!first) {
			first = 1;
		} else {
			if(estring_append(e, " ", 1) < 0)
				return -1;
		}
		rc = get_relative_dir(NULL, e, NULL, relative_to,
		                      tlist->tent->tnode.tupid,
		                      &clen);
		if (rc < 0 || clen < 0)
			return -1;
	}
	return 0;
}

struct node_var_entry *nodedb_get(struct node_vardb *v, const char *var, int varlen)
{
	struct string_tree *st;

	st = string_tree_search(&v->root, var, varlen);
	if(st) {
		struct node_var_entry *ve = container_of(st, struct node_var_entry, var);
		return ve;
	}
	return NULL;
}
