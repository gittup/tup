#include "string_tree.h"
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int string_tree_cmp(struct string_tree *st1, struct string_tree *st2)
{
	int result;
	result = name_cmp_n(st1->s, st2->s, st1->len);
	if(result == 0)
		result = st1->len - st2->len;
	return result;
}

RB_GENERATE(string_entries, string_tree, linkage, string_tree_cmp);

int string_tree_insert(struct string_entries *root, struct string_tree *st)
{
	if(RB_INSERT(string_entries, root, st) != NULL) {
		return -1;
	}
	return 0;
}

struct string_tree *string_tree_search(struct string_entries *root, const char *s,
				       int len)
{
	struct string_tree tmp;
	char buf[len+1];
	memcpy(buf, s, len);
	buf[len] = 0;
	tmp.s = buf;
	tmp.len = len;
	return RB_FIND(string_entries, root, &tmp);
}

int string_tree_add(struct string_entries *root, struct string_tree *st, const char *s)
{
	st->len = strlen(s);
	st->s = malloc(st->len + 1);
	if(!st->s) {
		perror("malloc");
		return -1;
	}

	memcpy(st->s, s, st->len);
	st->s[st->len] = 0;

	if(RB_INSERT(string_entries, root, st) != NULL) {
		free(st->s);
		return -1;
	}
	return 0;
}

void string_tree_free(struct string_entries *root, struct string_tree *st)
{
	RB_REMOVE(string_entries, root, st);
	free(st->s);
}
