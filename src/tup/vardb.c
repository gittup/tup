#include "vardb.h"
#include "string_tree.h"
#include "container.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int vardb_init(struct vardb *v)
{
	RB_INIT(&v->root);
	v->count = 0;
	return 0;
}

int vardb_close(struct vardb *v)
{
	struct string_tree *st;

	while((st = BSD_RB_ROOT(&v->root)) != NULL) {
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

	st = string_tree_search(&v->root, var, varlen);
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
			free(ve);
			return NULL;
		}
		memcpy(ve->var.s, var, varlen);
		ve->var.s[varlen] = 0;
		ve->vallen = strlen(value);
		ve->value = malloc(ve->vallen + 1);
		if(!ve->value) {
			perror("malloc");
			free(ve->var.s);
			free(ve);
			return NULL;
		}
		strcpy(ve->value, value);
		if(string_tree_insert(&v->root, &ve->var) < 0) {
			fprintf(stderr, "vardb_set: Error inserting into tree\n");
			free(ve->value);
			free(ve->var.s);
			free(ve);
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

int vardb_len(struct vardb *v, const char *var, int varlen)
{
	struct string_tree *st;

	st = string_tree_search(&v->root, var, varlen);
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

	st = string_tree_search(&v->root, var, varlen);
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

	st = string_tree_search(&v->root, var, varlen);
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
			if(extra_b && extra_b(veb) < 0)
				return -1;
			stb = RB_NEXT(string_entries, b, stb);
		} else if(!stb) {
			vea = container_of(sta, struct var_entry, var);
			if(extra_a && extra_a(vea) < 0)
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
				if(extra_a && extra_a(vea) < 0)
					return -1;
				sta = RB_NEXT(string_entries, a, sta);
			} else {
				if(extra_b && extra_b(veb) < 0)
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
