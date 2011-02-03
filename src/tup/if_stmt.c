#include "if_stmt.h"
#include <stdio.h>

#define IFMAX 0x80

void if_init(struct if_stmt *ifs)
{
	ifs->ifness = 0;
	ifs->level = 0;
}

int if_add(struct if_stmt *ifs, int is_true)
{
	if(ifs->level >= IFMAX) {
		fprintf(stderr, "Parse error: too many nested if statements\n");
		return -1;
	}
	if(ifs->level == 0)
		ifs->level = 1;
	else
		ifs->level <<= 1;
	if(!is_true)
		ifs->ifness |= ifs->level;
	return 0;
}

int if_else(struct if_stmt *ifs)
{
	if(ifs->level == 0) {
		fprintf(stderr, "Parse error: else statement outside of an if block\n");
		return -1;
	}
	ifs->ifness ^= ifs->level;
	return 0;
}

int if_endif(struct if_stmt *ifs)
{
	if(ifs->level == 0) {
		fprintf(stderr, "Parse error: endif statement outside of an if block\n");
		return -1;
	}
	ifs->ifness &= ~ifs->level;
	ifs->level >>= 1;
	return 0;
}

int if_true(struct if_stmt *ifs)
{
	if(ifs->ifness)
		return 0;
	return 1;
}

int if_check(struct if_stmt *ifs)
{
	if(ifs->level == 0)
		return 0;
	return -1;
}
