#ifndef tup_parser_h
#define tup_parser_h

#include "tupid.h"
struct node;
struct graph;

void parser_debug_run(void);
int parse(struct node *n, struct graph *g);

#endif
