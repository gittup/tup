#include "vardb.h"
#include "string_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct var_entry {
	struct string_tree var;
	char *value;
	int vallen;
};

int vardb_init(struct vardb *v)
{
	v->tree.rb_node = NULL;
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

int vardb_set(struct vardb *v, const char *var, const char *value)
{
	struct string_tree *st;
	struct var_entry *ve;

	st = string_tree_search(&v->tree, var, -1);
	if(st) {
		ve = container_of(st, struct var_entry, var);
		free(ve->value);
		ve->vallen = strlen(value);
		ve->value = malloc(ve->vallen + 1);
		if(!ve->value) {
			perror("malloc");
			return -1;
		}
		strcpy(ve->value, value);
		return 0;
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
			return -1;
		}
		strcpy(ve->var.s, var);
		ve->vallen = strlen(value);
		ve->value = malloc(ve->vallen + 1);
		if(!ve->value) {
			perror("malloc");
			return -1;
		}
		strcpy(ve->value, value);
		if(string_tree_insert(&v->tree, &ve->var) < 0) {
			fprintf(stderr, "vardb_set: Error inserting into tree\n");
			return -1;
		}
		return 0;
	}
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
		return vardb_set(v, var, value);
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

const char *vardb_get(struct vardb *v, const char *var)
{
	struct string_tree *st;

	st = string_tree_search(&v->tree, var, -1);
	if(st) {
		struct var_entry *ve = container_of(st, struct var_entry, var);
		return ve->value;
	}
	return NULL;
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
