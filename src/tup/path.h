#ifndef tup_path_h
#define tup_path_h

#include "tupid.h"
#include "tupid_tree.h"

int watch_path(tupid_t dt, int dfd, const char *file, struct tupid_entries *tree,
	       int (*callback)(tupid_t newdt, int dfd, const char *file));
int tup_scan(void);

#endif
