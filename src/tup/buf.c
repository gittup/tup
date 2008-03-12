#include "buf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int buf_cmp(const struct buf *b, const char *s)
{
	int x;

	for(x=0; x<b->len; x++) {
		if(s[x] == 0)
			return -1;
		if(b->s[x] != s[x])
			return -1;
	}
	if(s[x] != 0) {
		return -1;
	}
	return 0;
}

char *buf_to_string(const struct buf *b)
{
	char *tmp;

	tmp = malloc(b->len + 1);
	if(!tmp) {
		perror("malloc");
		return NULL;
	}
	memcpy(tmp, b->s, b->len);
	tmp[b->len] = 0;
	return tmp;
}
