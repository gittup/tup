#ifndef tup_parser_h
#define tup_parser_h

#include "tupid.h"
struct memdb;

int parse(tupid_t tupid, struct memdb *mdb);

#endif
