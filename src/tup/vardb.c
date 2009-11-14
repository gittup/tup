#include "vardb.h"
#include "string_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vardb_init(struct vardb *v)
{
	v->tree.rb_node = NULL;
	v->count = 0;
	return 0;
}

int vardb_close(struct vardb *v)
{
	struct rb_node *rbn;

	while((rbn = rb_first(&v->tree)) != NULL) {
		struct string_tree *st = rb_entry(rbn, struct string_tree, rbn);
		struct var_entry *ve = container_of(st, struct var_entry, var);
		rb_erase(rbn, &v->tree);
		free(st->s);
		free(ve->value);
		free(ve);
	}
	return 0;
}

int vardb_set(struct vardb *v, const char *var, const char *value,
	      struct tup_entry *tent)
{
	if(vardb_set2(v, var, -1, value, tent) == NULL)
		return -1;
	return 0;
}

struct var_entry *vardb_set2(struct vardb *v, const char *var, int varlen,
			     const char *value, struct tup_entry *tent)
{
	struct string_tree *st;
	struct var_entry *ve;

	st = string_tree_search(&v->tree, var, varlen);
	if(st) {
		ve = container_of(st, struct var_entry, var);
		free(ve->value);
		ve->vallen = strlen(value);
		ve->value = malloc(ve->vallen + 1);
		if(!ve->value) {
			perror("malloc");
			return NULL;
		}
		strcpy(ve->value, value);
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
			return NULL;
		}
		memcpy(ve->var.s, var, varlen);
		ve->var.s[varlen] = 0;
		ve->vallen = strlen(value);
		ve->value = malloc(ve->vallen + 1);
		if(!ve->value) {
			perror("malloc");
			return NULL;
		}
		strcpy(ve->value, value);
		if(string_tree_insert(&v->tree, &ve->var) < 0) {
			fprintf(stderr, "vardb_set: Error inserting into tree\n");
			return NULL;
		}
		ve->tent = tent;

		v->count++;
	}
	return ve;
}

int vardb_append(struct vardb *v, const char *var, const char *value)
{
	struct string_tree *st;

	st = string_tree_search(&v->tree, var, -1);
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

int vardb_len(struct vardb *v, const char *var, int varlen)
{
	struct string_tree *st;

	st = string_tree_search(&v->tree, var, varlen);
	if(st) {
		struct var_entry *ve = container_of(st, struct var_entry, var);
		return ve->vallen;
	}
	/* Variable not found: length of "" == 0 */
	return 0;
}

int vardb_copy(struct vardb *v, const char *var, int varlen, char **dest)
{
	struct string_tree *st;

	st = string_tree_search(&v->tree, var, varlen);
	if(st) {
		struct var_entry *ve = container_of(st, struct var_entry, var);
		memcpy(*dest, ve->value, ve->vallen);
		*dest += ve->vallen;
		return 0;
	}
	/* Variable not found: string is "" */
	return 0;
}

struct var_entry *vardb_get(struct vardb *v, const char *var, int varlen)
{
	struct string_tree *st;

	st = string_tree_search(&v->tree, var, varlen);
	if(st) {
		struct var_entry *ve = container_of(st, struct var_entry, var);
		return ve;
	}
	return NULL;
}

int vardb_compare(struct vardb *vdba, struct vardb *vdbb,
		  int (*extra_a)(struct var_entry *ve),
		  int (*extra_b)(struct var_entry *ve),
		  int (*same)(struct var_entry *vea, struct var_entry *veb))
{
	struct rb_node *na;
	struct rb_node *nb;
	struct string_tree *sta;
	struct string_tree *stb;
	struct var_entry *vea;
	struct var_entry *veb;
	struct rb_root *a = &vdba->tree;
	struct rb_root *b = &vdbb->tree;

	na = rb_first(a);
	nb = rb_first(b);

	while(na || nb) {
		if(!na) {
			stb = container_of(nb, struct string_tree, rbn);
			veb = container_of(stb, struct var_entry, var);
			if(extra_b && extra_b(veb) < 0)
				return -1;
			nb = rb_next(nb);
		} else if(!nb) {
			sta = container_of(na, struct string_tree, rbn);
			vea = container_of(sta, struct var_entry, var);
			if(extra_a && extra_a(vea) < 0)
				return -1;
			na = rb_next(na);
		} else {
			int rc;
			sta = container_of(na, struct string_tree, rbn);
			stb = container_of(nb, struct string_tree, rbn);
			vea = container_of(sta, struct var_entry, var);
			veb = container_of(stb, struct var_entry, var);
			rc = strcmp(sta->s, stb->s);
			if(rc == 0) {
				if(same && same(vea, veb) < 0)
					return -1;
				na = rb_next(na);
				nb = rb_next(nb);
			} else if(rc < 0) {
				if(extra_a && extra_a(vea) < 0)
					return -1;
				na = rb_next(na);
			} else {
				if(extra_b && extra_b(veb) < 0)
					return -1;
				nb = rb_next(nb);
			}
		}
	}
	return 0;
}

void vardb_dump(struct vardb *v)
{
	struct rb_node *rbn;

	rbn = rb_first(&v->tree);
	printf(" ----------- VARDB -----------\n");
	while(rbn) {
		struct string_tree *st = container_of(rbn, struct string_tree, rbn);
		struct var_entry *ve = container_of(st, struct var_entry, var);
		printf(" [%i] '%s' [33m=[0m [%i] '%s'\n", st->len, st->s, ve->vallen, ve->value);
		rbn = rb_next(rbn);
	}
}
