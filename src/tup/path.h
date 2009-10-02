#include "tupid.h"

struct rb_root;

int watch_path(tupid_t dt, int dfd, const char *file, struct rb_root *tree,
	       int (*callback)(tupid_t newdt, int dfd, const char *file));
int tup_scan(void);
